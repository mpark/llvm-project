//===--- SemaMatchPattern.cpp - Semantic Analysis for Patterns ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements semantic analysis for C++ patterns.
///
//===----------------------------------------------------------------------===//

#include "CheckExprLifetime.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/MatchPattern.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/EnterExpressionEvaluationContext.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/SemaObjC.h"
#include "clang/Sema/TemplateDeduction.h"
#include "llvm/ADT/ScopeExit.h"

using namespace clang;
using namespace sema;

static VarDecl *BuildVarDecl(Sema &SemaRef, SourceLocation Loc, QualType Type,
                             Expr *Init, bool IsConstexpr = false) {
  DeclContext *DC = SemaRef.CurContext;
  TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
  VarDecl *Decl = VarDecl::Create(SemaRef.Context, DC, Loc, Loc, /*Id=*/nullptr,
                                  Type, TInfo, SC_None);
  Decl->setImplicit();
  Decl->setConstexpr(IsConstexpr);
  // TODO: Consider ActOnInitializerError
  SemaRef.AddInitializerToDecl(Decl, Init, /*DirectInit=*/false);
  SemaRef.FinalizeDeclaration(Decl);
  return Decl;
}

static bool hasFailedVariableInitialization(const VarDecl *Declaration) {
  return Declaration->isInvalidDecl() || !Declaration->hasInit() ||
         Declaration->getInit()->containsErrors();
}

// Copied from SemaDeclCXX.cpp
static std::string printTemplateArgs(const PrintingPolicy &PrintingPolicy,
                                     TemplateArgumentListInfo &Args,
                                     const TemplateParameterList *Params) {
  SmallString<128> SS;
  llvm::raw_svector_ostream OS(SS);
  bool First = true;
  unsigned I = 0;
  for (auto &Arg : Args.arguments()) {
    if (!First)
      OS << ", ";
    Arg.getArgument().print(PrintingPolicy, OS,
                            TemplateParameterList::shouldIncludeTypeForArgument(
                                PrintingPolicy, Params, I));
    First = false;
    I++;
  }
  return std::string(OS.str());
}

// Copied from SemaDeclCXX.cpp
static bool lookupStdTypeTraitMember(Sema &S, LookupResult &TraitMemberLookup,
                                     SourceLocation Loc, StringRef Trait,
                                     TemplateArgumentListInfo &Args,
                                     unsigned DiagID,
                                     QualType *TraitType = nullptr) {
  auto DiagnoseMissing = [&] {
    if (DiagID)
      S.Diag(Loc, DiagID) << printTemplateArgs(S.Context.getPrintingPolicy(),
                                               Args, /*Params*/ nullptr);
    return true;
  };

  // FIXME: Factor out duplication with lookupPromiseType in SemaCoroutine.
  NamespaceDecl *Std = S.getStdNamespace();
  if (!Std)
    return DiagnoseMissing();

  // Look up the trait itself, within namespace std. We can diagnose various
  // problems with this lookup even if we've been asked to not diagnose a
  // missing specialization, because this can only fail if the user has been
  // declaring their own names in namespace std or we don't support the
  // standard library implementation in use.
  LookupResult Result(S, &S.PP.getIdentifierTable().get(Trait), Loc,
                      Sema::LookupOrdinaryName);
  if (!S.LookupQualifiedName(Result, Std))
    return DiagnoseMissing();
  if (Result.isAmbiguous())
    return true;

  ClassTemplateDecl *TraitTD = Result.getAsSingle<ClassTemplateDecl>();
  if (!TraitTD) {
    Result.suppressDiagnostics();
    NamedDecl *Found = *Result.begin();
    S.Diag(Loc, diag::err_std_type_trait_not_class_template) << Trait;
    S.Diag(Found->getLocation(), diag::note_declared_at);
    return true;
  }

  // Build the template-id.
  QualType TraitTy = S.CheckTemplateIdType(
      ElaboratedTypeKeyword::None, TemplateName(TraitTD), Loc, Args,
      /*Scope=*/nullptr, /*ForNestedNameSpecifier=*/false);
  if (TraitTy.isNull())
    return true;
  if (TraitType)
    *TraitType = TraitTy;
  if (!S.isCompleteType(Loc, TraitTy)) {
    if (DiagID)
      S.RequireCompleteType(
          Loc, TraitTy, DiagID,
          printTemplateArgs(S.Context.getPrintingPolicy(), Args,
                            TraitTD->getTemplateParameters()));
    return true;
  }

  CXXRecordDecl *RD = TraitTy->getAsCXXRecordDecl();
  assert(RD && "specialization of class template is not a class?");

  // Look up the member of the trait type.
  S.LookupQualifiedName(TraitMemberLookup, RD);
  return TraitMemberLookup.isAmbiguous();
}

// Copied from SemaDeclCXX.cpp
static TemplateArgumentLoc
getTrivialIntegralTemplateArgument(Sema &S, SourceLocation Loc, QualType T,
                                   uint64_t I) {
  TemplateArgument Arg(S.Context, S.Context.MakeIntValue(I, T), T);
  return S.getTrivialTemplateArgumentLoc(Arg, T, Loc);
}

// Copied from SemaDeclCXX.cpp
static TemplateArgumentLoc
getTrivialTypeTemplateArgument(Sema &S, SourceLocation Loc, QualType T) {
  return S.getTrivialTemplateArgumentLoc(TemplateArgument(T), QualType(), Loc);
}

namespace {
struct AlternativeTraitsInfo {
  QualType Type;
  CXXRecordDecl *Record = nullptr;
  unsigned Size = 0;
  bool IsExhaustive = true;
  bool IsBuiltinPointer = false;
  bool IsOpen = false;
  llvm::SmallVector<QualType, 4> Alternatives;
  llvm::SmallVector<bool, 4> Projectable;
};
} // namespace

static bool lookupAlternativeTraits(Sema &S, SourceLocation Loc,
                                    QualType SubjectType,
                                    AlternativeTraitsInfo &Info) {
  SubjectType = SubjectType.getNonReferenceType().getUnqualifiedType();
  if (const auto *Pointer = SubjectType->getAs<PointerType>();
      Pointer && !Pointer->getPointeeType()->isVoidType()) {
    Info.Size = 2;
    Info.IsBuiltinPointer = true;
    Info.Alternatives = {S.Context.VoidTy, Pointer->getPointeeType()};
    Info.Projectable = {false, true};
    return false;
  }

  TemplateArgumentListInfo TraitArgs(Loc, Loc);
  TraitArgs.addArgument(getTrivialTypeTemplateArgument(S, Loc, SubjectType));

  LookupResult SizeLookup(S, S.PP.getIdentifierInfo("size"), Loc,
                          Sema::LookupOrdinaryName);
  if (lookupStdTypeTraitMember(S, SizeLookup, Loc, "alternative_traits",
                               TraitArgs, diag::err_alternative_traits_missing,
                               &Info.Type))
    return true;

  Info.Record = Info.Type->getAsCXXRecordDecl();
  if (!Info.Record)
    return true;

  if (SizeLookup.empty()) {
    LookupResult TypeLookup(S, S.PP.getIdentifierInfo("type"), Loc,
                            Sema::LookupOrdinaryName);
    S.LookupQualifiedName(TypeLookup, Info.Record);
    if (TypeLookup.empty()) {
      S.Diag(Loc, diag::err_alternative_traits_member_missing)
          << SubjectType << "either 'size' or 'type'";
      return true;
    }
    Info.IsOpen = true;
    Info.IsExhaustive = false;
    return false;
  }

  ExprResult SizeExpr =
      S.BuildDeclarationNameExpr(CXXScopeSpec(), SizeLookup, false);
  if (SizeExpr.isInvalid())
    return true;
  llvm::APSInt SizeValue(32);
  struct SizeDiagnoser : Sema::VerifyICEDiagnoser {
    QualType Type;
    explicit SizeDiagnoser(QualType Type) : Type(Type) {}
    Sema::SemaDiagnosticBuilder diagnoseNotICE(Sema &S,
                                               SourceLocation Loc) override {
      return S.Diag(Loc, diag::err_alternative_traits_size_not_constant)
             << Type;
    }
  } Diagnoser(SubjectType);
  if (S.VerifyIntegerConstantExpression(SizeExpr.get(), &SizeValue, Diagnoser)
          .isInvalid())
    return true;
  Info.Size = SizeValue.getLimitedValue(UINT_MAX);

  LookupResult ExhaustiveLookup(S, S.PP.getIdentifierInfo("is_exhaustive"), Loc,
                                Sema::LookupOrdinaryName);
  S.LookupQualifiedName(ExhaustiveLookup, Info.Record);
  if (!ExhaustiveLookup.empty()) {
    ExprResult ExhaustiveExpr =
        S.BuildDeclarationNameExpr(CXXScopeSpec(), ExhaustiveLookup, false);
    if (ExhaustiveExpr.isInvalid())
      return true;
    llvm::APSInt ExhaustiveValue(1);
    if (S.VerifyIntegerConstantExpression(ExhaustiveExpr.get(),
                                          &ExhaustiveValue)
            .isInvalid())
      return true;
    Info.IsExhaustive = !ExhaustiveValue.isZero();
  }

  LookupResult ProjectionTypeLookup(
      S, S.PP.getIdentifierInfo("projection_type"), Loc,
      Sema::LookupOrdinaryName);
  S.LookupQualifiedName(ProjectionTypeLookup, Info.Record);
  auto *ProjectionTypeTemplate =
      ProjectionTypeLookup.getAsSingle<TypeAliasTemplateDecl>();

  Info.Alternatives.reserve(Info.Size);
  Info.Projectable.reserve(Info.Size);
  for (unsigned I = 0; I < Info.Size; ++I) {
    TemplateArgumentListInfo Args(Loc, Loc);
    Args.addArgument(
        getTrivialIntegralTemplateArgument(S, Loc, S.Context.getSizeType(), I));
    QualType ProjectionType;
    bool IsProjectable = false;
    if (ProjectionTypeTemplate) {
      Sema::SFINAETrap Trap(S, /*WithAccessChecking=*/true);
      ProjectionType = S.CheckTemplateIdType(
          ElaboratedTypeKeyword::None, TemplateName(ProjectionTypeTemplate),
          Loc, Args, nullptr, false);
      IsProjectable = !ProjectionType.isNull() && !Trap.hasErrorOccurred();
    }
    Info.Alternatives.push_back(IsProjectable ? ProjectionType
                                              : S.Context.VoidTy);
    Info.Projectable.push_back(IsProjectable);
  }
  return false;
}

static std::optional<unsigned>
lookupAlternativeName(Sema &S, SourceLocation Loc, QualType SubjectType,
                      const AlternativeTraitsInfo &Info, IdentifierInfo *Name) {
  if (Info.IsBuiltinPointer) {
    S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
    return std::nullopt;
  }

  LookupResult NamesLookup(S, S.PP.getIdentifierInfo("names"), Loc,
                           Sema::LookupOrdinaryName);
  S.LookupQualifiedName(NamesLookup, Info.Record);
  CXXRecordDecl *NamesRecord = nullptr;
  for (NamedDecl *Decl : NamesLookup) {
    auto *Record = dyn_cast<CXXRecordDecl>(Decl->getUnderlyingDecl());
    if (Record && !Record->isInjectedClassName()) {
      NamesRecord = Record;
      break;
    }
  }
  if (!NamesRecord) {
    S.Diag(Loc, diag::err_alternative_traits_member_missing)
        << SubjectType << "names";
    return std::nullopt;
  }
  QualType NamesType = S.Context.getTagType(ElaboratedTypeKeyword::None,
                                            NestedNameSpecifier(std::nullopt),
                                            NamesRecord,
                                            /*OwnsTag=*/false);
  if (S.RequireCompleteType(Loc, NamesType,
                            diag::err_alternative_traits_member_missing,
                            SubjectType, "complete names type"))
    return std::nullopt;
  NamesRecord = NamesType->getAsCXXRecordDecl()->getDefinition();

  LookupResult NameLookup(S, Name, Loc, Sema::LookupOrdinaryName);
  S.LookupQualifiedName(NameLookup, NamesRecord);
  ValueDecl *NameDecl = nullptr;
  for (NamedDecl *Decl : NameLookup)
    if ((NameDecl = dyn_cast<ValueDecl>(Decl->getUnderlyingDecl())))
      break;
  if (!NameDecl) {
    S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
    return std::nullopt;
  }
  llvm::APSInt Value(32);
  if (auto *Enumerator = dyn_cast<EnumConstantDecl>(NameDecl)) {
    Value = Enumerator->getInitVal();
  } else {
    Expr *NameExpr =
        S.BuildDeclRefExpr(NameDecl, NameDecl->getType(), VK_LValue, Loc);
    if (S.VerifyIntegerConstantExpression(NameExpr, &Value).isInvalid())
      return std::nullopt;
  }
  unsigned Index = Value.getLimitedValue(UINT_MAX);
  if (Index >= Info.Size) {
    S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
    return std::nullopt;
  }
  return Index;
}

static ExprResult buildAlternativeTraitsCall(
    Sema &S, SourceLocation Loc, const AlternativeTraitsInfo &Info,
    StringRef Member, Expr *Subject,
    std::optional<unsigned> AlternativeIndex = std::nullopt) {
  if (Info.IsBuiltinPointer) {
    if (Member == "index")
      return S.PerformContextuallyConvertToBool(Subject);
    assert(Member == "get" && AlternativeIndex == 1 &&
           "unexpected built-in pointer alternative operation");
    return S.CreateBuiltinUnaryOp(Loc, UO_Deref, Subject);
  }

  LookupResult MemberLookup(S, S.PP.getIdentifierInfo(Member), Loc,
                            Sema::LookupOrdinaryName);
  S.LookupQualifiedName(MemberLookup, Info.Record);
  if (MemberLookup.empty())
    return ExprError();

  CXXScopeSpec SS;
  SS.MakeTrivial(S.Context, NestedNameSpecifier(Info.Type.getTypePtr()), Loc);
  ExprResult Callee;
  if (AlternativeIndex) {
    TemplateArgumentListInfo Args(Loc, Loc);
    Args.addArgument(getTrivialIntegralTemplateArgument(
        S, Loc, S.Context.getSizeType(), *AlternativeIndex));
    Callee = S.BuildTemplateIdExpr(SS, SourceLocation(), MemberLookup,
                                   /*RequiresADL=*/false, &Args);
  } else {
    Callee = S.BuildDeclarationNameExpr(SS, MemberLookup, false);
  }
  if (Callee.isInvalid())
    return ExprError();
  return S.BuildCallExpr(nullptr, Callee.get(), Loc, Subject, Loc);
}

static ExprResult buildOpenAlternativeTraitsGetCall(
    Sema &S, SourceLocation Loc, const AlternativeTraitsInfo &Info,
    QualType SubjectType, QualType RequestedType, Expr *Subject) {
  LookupResult GetLookup(S, S.PP.getIdentifierInfo("get"), Loc,
                         Sema::LookupOrdinaryName);
  S.LookupQualifiedName(GetLookup, Info.Record);
  if (GetLookup.empty()) {
    S.Diag(Loc, diag::err_alternative_traits_member_missing)
        << SubjectType << "'get'";
    return ExprError();
  }

  CXXScopeSpec SS;
  SS.MakeTrivial(S.Context, NestedNameSpecifier(Info.Type.getTypePtr()), Loc);
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(getTrivialTypeTemplateArgument(S, Loc, RequestedType));
  ExprResult Callee = S.BuildTemplateIdExpr(SS, SourceLocation(), GetLookup,
                                            /*RequiresADL=*/false, &Args);
  if (Callee.isInvalid())
    return ExprError();
  return S.BuildCallExpr(nullptr, Callee.get(), Loc, Subject, Loc);
}

static ExprResult buildTypeId(Sema &S, SourceLocation Loc, QualType Type) {
  ParsedType Parsed = ParsedType::make(Type);
  return S.ActOnCXXTypeid(Loc, Loc, /*isType=*/true, Parsed.getAsOpaquePtr(),
                          Loc);
}

ExprResult Sema::ActOnMatchSubject(Expr *Subject, VarDecl *&HoldingVar) {
  if (Subject->getType()->isVoidType())
    return Subject;

  bool IsConstant = Subject->isCXX11ConstantExpr(Context);
  bool MaterializeConstantPrValue =
      IsConstant && Subject->isPRValue() && Subject->getType()->isRecordType();
  if (IsConstant && !MaterializeConstantPrValue)
    return Subject;

  QualType Deduced = Subject->refersToBitField() ? Context.getAutoDeductType()
                     : MaterializeConstantPrValue
                         ? Subject->getType()
                         : Context.getAutoRRefDeductType();
  VarDecl *VD = BuildVarDecl(*this, Subject->getExprLoc(), Deduced, Subject,
                             MaterializeConstantPrValue);
  if (VD->isInvalidDecl()) {
    return ExprError();
  }
  HoldingVar = VD;
  ExprResult Ref = BuildDeclRefExpr(
      HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue,
      Subject->getExprLoc());
  if (Ref.isInvalid() || Subject->isLValue())
    return Ref;
  return ImplicitCastExpr::Create(Context, Ref.get()->getType(), CK_NoOp,
                                  Ref.get(), nullptr, VK_XValue,
                                  FPOptionsOverride());
}

namespace {

static void forEachDeclarationPattern(
    MatchPattern *Pattern,
    llvm::function_ref<void(DeclarationPattern *)> Callback) {
  if (!Pattern)
    return;
  if (auto *P = dyn_cast<DeclarationPattern>(Pattern)) {
    Callback(P);
    return;
  }
  for (MatchPattern *Child : Pattern->children())
    forEachDeclarationPattern(Child, Callback);
}

static bool isMoveInitialized(const VarDecl *Declaration) {
  if (Declaration->isInvalidDecl() || !Declaration->hasInit())
    return false;

  const Expr *Init = Declaration->getInit();
  while (true) {
    Init = Init->IgnoreParens();
    if (const auto *E = dyn_cast<ExprWithCleanups>(Init))
      Init = E->getSubExpr();
    else if (const auto *E = dyn_cast<CXXBindTemporaryExpr>(Init))
      Init = E->getSubExpr();
    else if (const auto *E = dyn_cast<MaterializeTemporaryExpr>(Init))
      Init = E->getSubExpr();
    else if (const auto *E = dyn_cast<ArrayInitLoopExpr>(Init))
      Init = E->getSubExpr();
    else
      break;
  }

  const auto *Construct = dyn_cast<CXXConstructExpr>(Init);
  return Construct && Construct->getConstructor()->isMoveConstructor();
}

} // namespace

void Sema::CheckGuardedMatchPattern(MatchPattern *Pattern) {
  forEachDeclarationPattern(Pattern, [&](DeclarationPattern *P) {
    VarDecl *Declaration = P->getDeclaration();
    if (isMoveInitialized(Declaration))
      Diag(P->getBeginLoc(), diag::err_guarded_declaration_pattern_move)
          << Declaration->getType();
  });
}

StmtResult Sema::ActOnMatchExprHandler(TypeLoc OrigResultType, QualType &RetTy,
                                       ExprResult ER) {
  if (ER.isInvalid()) {
    return StmtError();
  }
  Expr *E = ER.get();
  SourceLocation Loc = E->getBeginLoc();
  const AutoType *HandlerAuto = E->getType()->getContainedAutoType();
  bool HasDependentAuto =
      HandlerAuto &&
      (!HandlerAuto->isDeduced() || HandlerAuto->getDeducedType().isNull() ||
       HandlerAuto->getDeducedType()->isDependentType());
  if (E->isTypeDependent() || HasDependentAuto || RetTy->isDependentType()) {
    if (OrigResultType.getType()->getContainedAutoType())
      RetTy = Context.DependentTy;
    return E;
  }
  if (isa<CXXThrowExpr>(E->IgnoreParenImpCasts())) {
    if (ER.isInvalid())
      return StmtError();
    return ER.get();
  }
  if (const AutoType *AT = RetTy->getContainedAutoType()) {
    QualType Deduced;
    if (DeduceAutoTypeFromExpr(OrigResultType, Loc, E, Deduced, AT)) {
      return StmtError();
    }
    RetTy = Deduced;
  }
  if (RetTy->isVoidType()) {
    ExprResult Result = E;
    Result = IgnoredValueConversions(Result.get());
    if (Result.isInvalid())
      return StmtError();
    E = Result.get();
    E = ImpCastExprToType(E, Context.VoidTy, CK_ToVoid).get();
  }
  Sema::NamedReturnInfo NRInfo = getNamedReturnInfo(E);
  auto Entity = InitializedEntity::InitializeStmtExprResult(Loc, RetTy);
  ER = PerformMoveOrCopyInitialization(Entity, NRInfo, E);
  if (ER.isInvalid())
    return StmtError();
  E = ER.get();
  CheckReturnValExpr(E, RetTy, Loc);
  ER = ActOnFinishFullExpr(E, Loc, /*DiscardedValue=*/false);
  if (ER.isInvalid())
    return StmtError();
  return ER.get();
}

ExprResult Sema::ActOnMatchTestExpr(
    VarDecl *HoldingVar, Expr *Subject, SourceLocation MatchLoc,
    MatchPattern *Pattern, MatchPatternInstantiation *Instantiation,
    SourceLocation IfLoc, MatchGuard Guard, bool PatternIsIrrefutable,
    bool NeedsCaseInstantiation,
    ArrayRef<MatchTestInstantiation> Instantiations) {
  return new (Context) MatchTestExpr(
      Context, HoldingVar, Subject, MatchLoc, Pattern, Instantiation, IfLoc,
      Guard, PatternIsIrrefutable, NeedsCaseInstantiation, Instantiations);
}

ExprResult Sema::ActOnMatchSelectExpr(
    VarDecl *HoldingVar, Expr *Subject, SourceLocation MatchLoc,
    bool IsConstexpr, TypeLoc OrigResultType, QualType RetTy,
    SmallVectorImpl<MatchCase> &SourceCases, SourceRange Braces,
    bool ExpandDeferredCases, bool RequireFirstCaseViable,
    std::optional<ArrayRef<MatchCaseInstantiation>> Instantiations) {
  if (ExpandDeferredCases) {
    auto *E = MatchSelectExpr::Create(
        Context, HoldingVar, Subject, MatchLoc, IsConstexpr,
        RequireFirstCaseViable, OrigResultType, RetTy, SourceCases, {}, Braces);
    return ExpandDeferredMatchSelectExpr(E);
  }

  SmallVector<MatchCaseInstantiation, 32> CaseInstantiations;
  if (Instantiations) {
    CaseInstantiations.append(Instantiations->begin(), Instantiations->end());
  } else {
    CaseInstantiations.reserve(SourceCases.size());
    for (auto [Index, Case] : llvm::enumerate(SourceCases))
      CaseInstantiations.push_back({Case.Pattern, Case.IfLoc, Case.Guard,
                                    Case.Handler, static_cast<unsigned>(Index),
                                    Case.PatternInstantiation});
  }

  CheckMatchSelectExhaustiveness(Subject, SourceCases, CaseInstantiations);
  return MatchSelectExpr::Create(Context, HoldingVar, Subject, MatchLoc,
                                 IsConstexpr, RequireFirstCaseViable,
                                 OrigResultType, RetTy, SourceCases,
                                 CaseInstantiations, Braces);
}

ActionResult<MatchPattern *>
Sema::ActOnWildcardPattern(SourceLocation WildcardLoc) {
  return new (Context) WildcardPattern(WildcardLoc);
}

ActionResult<MatchPattern *>
Sema::ActOnExpressionPattern(Expr *E, bool IsPackExpansion) {
  return new (Context) ExpressionPattern(E, IsPackExpansion);
}

ActionResult<MatchPattern *>
Sema::ActOnDeclarationPattern(VarDecl *Declaration, SourceRange WrittenRange) {
  return new (Context) DeclarationPattern(Declaration, WrittenRange);
}

ActionResult<MatchPattern *> Sema::ActOnTypePattern(TypeSourceInfo *TInfo) {
  return new (Context) TypePattern(TInfo);
}

ActionResult<MatchPattern *>
Sema::ActOnBracedAlternativePattern(SourceRange Braces,
                                    MatchPattern *SubPattern) {
  return new (Context) AlternativePattern(Braces, SubPattern);
}

ActionResult<MatchPattern *> Sema::ActOnNamedAlternativePattern(
    SourceRange Braces, SourceRange NameRange, IdentifierInfo *Name,
    SourceLocation ColonLoc, MatchPattern *SubPattern) {
  return new (Context)
      AlternativePattern(Braces, NameRange, Name, ColonLoc, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnEmptyAlternativePattern(SourceRange Braces) {
  return new (Context) AlternativePattern(Braces);
}

ActionResult<MatchPattern *>
Sema::ActOnDecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares) {
  return DecompositionPattern::Create(Context, Patterns, Squares);
}

static MatchProjection *
findMatchProjection(Sema &S, Sema::MatchProjectionCache *Cache,
                    const Expr *Subject, MatchProjection::ProjectionKind Kind,
                    QualType Discriminator = QualType(), unsigned Arity = 0) {
  if (!Cache)
    return nullptr;
  for (const Sema::MatchProjectionCache::Entry &Entry : Cache->Entries) {
    if (Entry.Subject != Subject || Entry.Projection->getKind() != Kind ||
        Entry.Arity != Arity || Entry.Path != Cache->CurrentProjectionPath)
      continue;
    if (Discriminator.isNull() == Entry.Discriminator.isNull() &&
        (Discriminator.isNull() ||
         S.Context.hasSameType(Discriminator, Entry.Discriminator)))
      return Entry.Projection;
  }
  return nullptr;
}

static MatchProjection *
createMatchProjection(Sema &S, Sema::MatchProjectionCache *Cache,
                      const Expr *Subject, MatchProjection::ProjectionKind Kind,
                      QualType Discriminator = QualType(), unsigned Arity = 0) {
  auto *Projection = new (S.Context) MatchProjection(Kind);
  if (Cache)
    Cache->Entries.push_back({Subject, Projection, Discriminator, Arity,
                              Cache->CurrentProjectionPath});
  return Projection;
}

static MatchProjection *
findAlternativeDiscriminatorProjection(Sema::MatchProjectionCache *Cache,
                                       const Expr *Subject) {
  if (!Cache)
    return nullptr;
  for (const Sema::MatchProjectionCache::Entry &Entry : Cache->Entries) {
    if (Entry.Subject == Subject &&
        Entry.Projection->getKind() == MatchProjection::AlternativeProjection &&
        Entry.Path == Cache->CurrentProjectionPath)
      return Entry.Projection;
  }
  return nullptr;
}

static void appendProjectionPath(const MatchPattern *Pattern,
                                 SmallVectorImpl<unsigned> &Path,
                                 Sema::MatchPatternState &State) {
  if (Pattern->getMatchPatternClass() ==
      MatchPattern::AlternativePatternClass) {
    const MatchPatternInfo &Info =
        State.get(const_cast<MatchPattern *>(Pattern));
    for (unsigned Index : Info.SelectedAlternatives)
      Path.push_back(Index + 1);
  }
  for (const MatchPattern *Child : Pattern->children())
    appendProjectionPath(Child, Path, State);
}

static bool isExactDeclarationPatternMatch(Sema &S, Expr *Subject,
                                           QualType PatternType) {
  ImplicitConversionSequence ICS = S.TryCopyInitializationConversion(
      Subject, PatternType, /*SuppressUserConversions=*/false,
      /*InOverloadResolution=*/true,
      /*AllowObjCWritebackConversion=*/false);
  return ICS.isStandard() && ICS.Standard.getRank() == ICR_Exact_Match;
}

static bool isDeducedDeclarationPatternApplicable(Sema &S, VarDecl *Declaration,
                                                  Expr *Subject) {
  if (!Declaration->getType()->isUndeducedType())
    return true;

  QualType DeducedType;
  TemplateDeductionInfo Info(Subject->getExprLoc());
  Sema::SFINAETrap Trap(S, /*WithAccessChecking=*/true);
  TemplateDeductionResult Result =
      S.DeduceAutoType(Declaration->getTypeSourceInfo()->getTypeLoc(), Subject,
                       DeducedType, Info);
  return Result == TemplateDeductionResult::Success &&
         !Trap.hasErrorOccurred() &&
         isExactDeclarationPatternMatch(S, Subject, DeducedType);
}

static bool isDecompositionDeclarationPatternApplicable(
    Sema &S, DecompositionDecl *Declaration, Expr *Subject) {
  Sema::SFINAETrap Trap(S, /*WithAccessChecking=*/true);
  UnsignedOrNone ElementCount = S.GetDecompositionElementCount(
      Subject->getType().getNonReferenceType(), Declaration->getLocation());
  if (!ElementCount || Trap.hasErrorOccurred())
    return false;

  ArrayRef<BindingDecl *> Bindings = Declaration->bindings();
  bool HasPack = llvm::any_of(Bindings, [](BindingDecl *Binding) {
    return Binding->isParameterPack();
  });
  return HasPack ? *ElementCount >= Bindings.size() - 1
                 : *ElementCount == Bindings.size();
}

static ExprResult buildMatchProjectionCondition(Sema &S,
                                                MatchProjection *Projection,
                                                SourceLocation Loc) {
  if (Expr *Condition = Projection->getConditionExpr())
    return Condition;
  VarDecl *ConditionVar = Projection->getConditionVar();
  Expr *ConditionRef =
      S.BuildDeclRefExpr(ConditionVar, S.Context.BoolTy, VK_LValue, Loc);
  ExprResult Condition = S.CheckBooleanCondition(Loc, ConditionRef);
  if (Condition.isUsable())
    Projection->setConditionExpr(Condition.get());
  return Condition;
}

static bool isReusableDecompositionDeclaration(const VarDecl *Declaration) {
  const auto *Decomposition = dyn_cast<DecompositionDecl>(Declaration);
  return Decomposition && Declaration->getType()->isRValueReferenceType() &&
         Declaration->getType().getNonReferenceType().getQualifiers().empty() &&
         Declaration->getType()->getContainedAutoType();
}

enum class CastProjectionResult { NotApplicable, Success, Error };

static bool isDynamicCastDeclaration(Sema &S, SourceLocation Loc,
                                     QualType SubjectType, QualType PatternType,
                                     bool &ProjectsPointer) {
  QualType SourceClass = SubjectType;
  QualType TargetClass = PatternType;
  ProjectsPointer = false;

  if (SubjectType->isPointerType() && PatternType->isPointerType()) {
    SourceClass = SubjectType->getPointeeType();
    TargetClass = PatternType->getPointeeType();
    ProjectsPointer = true;
  } else if (!SubjectType->isRecordType() || !PatternType->isRecordType()) {
    return false;
  }

  SourceClass = SourceClass.getUnqualifiedType();
  TargetClass = TargetClass.getUnqualifiedType();
  CXXRecordDecl *SourceRecord = SourceClass->getAsCXXRecordDecl();
  CXXRecordDecl *TargetRecord = TargetClass->getAsCXXRecordDecl();
  return SourceRecord && TargetRecord && SourceRecord->isPolymorphic() &&
         S.IsDerivedFrom(Loc, TargetClass, SourceClass);
}

static Expr *asValueKind(Sema &S, Expr *E, ExprValueKind ValueKind) {
  if (ValueKind == VK_LValue)
    return E;
  return ImplicitCastExpr::Create(S.Context, E->getType(), CK_NoOp, E, nullptr,
                                  ValueKind, FPOptionsOverride());
}

static void setCastProjection(Sema::MatchPatternState &State,
                              MatchPattern *Pattern,
                              MatchProjection *Projection) {
  State.get(Pattern).Projection = Projection;
}

static CastProjectionResult buildDeclarationLikeCastProjection(
    Sema &S, Expr *Subject, MatchPattern *Pattern, QualType PatternType,
    Sema::MatchPatternState &State, Sema::MatchProjectionCache *Cache) {
  SourceLocation Loc = Pattern->getBeginLoc();
  QualType TargetType = PatternType.getNonReferenceType().getUnqualifiedType();
  if (MatchProjection *Projection = findMatchProjection(
          S, Cache, Subject, MatchProjection::CastProjection, TargetType)) {
    setCastProjection(State, Pattern, Projection);
    return CastProjectionResult::Success;
  }

  ExprValueKind SubjectValueKind = Subject->getValueKind();
  QualType Deduced = S.Context.getAutoRRefDeductType();
  VarDecl *HoldingVar = BuildVarDecl(S, Loc, Deduced, Subject);
  if (HoldingVar->isInvalidDecl())
    return CastProjectionResult::Error;
  QualType SubjectType = HoldingVar->getType().getNonReferenceType();
  Expr *HoldingRef =
      S.BuildDeclRefExpr(HoldingVar, SubjectType, VK_LValue, Loc);
  Expr *ForwardedRef = asValueKind(S, HoldingRef, SubjectValueKind);

  DeclarationNameInfo TryCastNameInfo(S.PP.getIdentifierInfo("try_cast"), Loc);
  OverloadCandidateSet CandidateSet(Loc, OverloadCandidateSet::CSK_Normal);
  ExprResult CastExpr = S.BuildTryCastCall(Loc, TryCastNameInfo, &CandidateSet,
                                           TargetType, ForwardedRef);
  bool DereferenceResult = true;

  if (CastExpr.isUnset()) {
    bool ProjectsPointer = false;
    if (!isDynamicCastDeclaration(S, Loc, SubjectType, TargetType,
                                  ProjectsPointer))
      return CastProjectionResult::NotApplicable;

    QualType DynamicTargetType = TargetType;
    Expr *CastOperand = HoldingRef;
    if (!ProjectsPointer) {
      DynamicTargetType =
          DynamicTargetType.withCVRQualifiers(SubjectType.getCVRQualifiers());
      DynamicTargetType = S.Context.getPointerType(DynamicTargetType);
      ExprResult AddrOf =
          S.ActOnUnaryOp(S.getCurScope(), Loc, tok::TokenKind::amp, HoldingRef);
      if (AddrOf.isInvalid())
        return CastProjectionResult::Error;
      CastOperand = AddrOf.get();
    } else {
      DereferenceResult = false;
    }

    TypeSourceInfo *TSI =
        S.Context.getTrivialTypeSourceInfo(DynamicTargetType, Loc);
    CastExpr =
        S.BuildCXXNamedCast({}, tok::kw_dynamic_cast, TSI, CastOperand, {}, {});
  }

  if (CastExpr.isInvalid())
    return CastProjectionResult::Error;

  MatchProjection *Projection = createMatchProjection(
      S, Cache, Subject, MatchProjection::CastProjection, TargetType);
  setCastProjection(State, Pattern, Projection);
  Projection->setHoldingVar(HoldingVar);

  VarDecl *CastVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), CastExpr.get());
  if (CastVar->isInvalidDecl())
    return CastProjectionResult::Error;
  Projection->setIntermediateVar(CastVar);
  Expr *CastRef = S.BuildDeclRefExpr(
      CastVar, CastVar->getType().getNonReferenceType(), VK_LValue, Loc);

  ExprResult RawCondition = S.CheckBooleanCondition(Loc, CastRef);
  if (RawCondition.isInvalid())
    return CastProjectionResult::Error;
  VarDecl *ConditionVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), RawCondition.get());
  if (ConditionVar->isInvalidDecl())
    return CastProjectionResult::Error;
  Projection->setConditionVar(ConditionVar);
  ExprResult Condition = buildMatchProjectionCondition(S, Projection, Loc);
  if (Condition.isInvalid())
    return CastProjectionResult::Error;

  if (TargetType->isVoidType())
    return CastProjectionResult::Success;

  ExprResult Projected = CastRef;
  if (DereferenceResult) {
    Projected =
        S.ActOnUnaryOp(S.getCurScope(), Loc, tok::TokenKind::star, CastRef);
    if (Projected.isInvalid())
      return CastProjectionResult::Error;
  }
  ExprValueKind ProjectedValueKind = SubjectValueKind;
  Expr *ProjectedExpr =
      asValueKind(S, Projected.get(), ProjectedValueKind);
  QualType ProjectedType = ProjectedExpr->refersToBitField()
                               ? S.Context.getAutoDeductType()
                               : S.Context.getAutoRRefDeductType();
  VarDecl *ProjectedVar = BuildVarDecl(S, Loc, ProjectedType, ProjectedExpr);
  if (ProjectedVar->isInvalidDecl())
    return CastProjectionResult::Error;
  Projection->setProjectedVar(ProjectedVar);
  Expr *ProjectedRef = S.BuildDeclRefExpr(
      ProjectedVar, ProjectedVar->getType().getNonReferenceType(), VK_LValue,
      Loc);
  ProjectedRef = asValueKind(S, ProjectedRef, ProjectedValueKind);
  Projection->setProjectedExpr(ProjectedRef);
  return CastProjectionResult::Success;
}

static QualType getOpenAlternativeRequestedType(MatchPattern *Pattern) {
  if (auto *Declaration = dyn_cast<DeclarationPattern>(Pattern))
    return Declaration->getDeclaration()->getType();
  if (auto *Type = dyn_cast<TypePattern>(Pattern))
    return Type->getType();
  return {};
}

static bool
checkOpenAlternativePattern(Sema &S, Expr *Subject, AlternativePattern *Pattern,
                            const AlternativeTraitsInfo &Traits,
                            Sema::MatchPatternState &State,
                            Sema::MatchProjectionCache *ProjectionCache) {
  SourceLocation Loc = Pattern->getBeginLoc();
  QualType SubjectType = Subject->getType().getNonReferenceType();
  if (Pattern->isNamed()) {
    S.Diag(Pattern->getDiscriminatorRange().getBegin(),
           diag::err_alternative_name_not_found)
        << Pattern->getName() << SubjectType;
    return true;
  }

  MatchPattern *SubPattern = Pattern->getSubPattern();
  bool IsProjectableWildcard =
      SubPattern &&
      SubPattern->getMatchPatternClass() == MatchPattern::WildcardPatternClass;
  QualType RequestedType =
      SubPattern ? getOpenAlternativeRequestedType(SubPattern) : QualType();
  if (!Pattern->isEmpty() && !IsProjectableWildcard &&
      (RequestedType.isNull() || RequestedType->getContainedAutoType() ||
       RequestedType->isVoidType())) {
    S.Diag(Loc, diag::err_open_alternative_pattern_not_type_directed)
        << SubjectType;
    return true;
  }

  MatchProjection *SharedDiscriminator =
      findAlternativeDiscriminatorProjection(ProjectionCache, Subject);
  MatchProjection *Projection = nullptr;
  if (SharedDiscriminator) {
    Projection = createMatchProjection(
        S, /*Cache=*/nullptr, Subject, MatchProjection::AlternativeProjection,
        RequestedType.isNull() ? QualType() : RequestedType);
    Projection->setHoldingVar(SharedDiscriminator->getHoldingVar());
    Projection->setIntermediateVar(SharedDiscriminator->getIntermediateVar());
  } else {
    Projection = createMatchProjection(
        S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
        RequestedType.isNull() ? QualType() : RequestedType);
    VarDecl *HoldingVar =
        BuildVarDecl(S, Loc, S.Context.getAutoRRefDeductType(), Subject);
    if (HoldingVar->isInvalidDecl())
      return true;
    Projection->setHoldingVar(HoldingVar);
    Expr *HoldingRef = S.BuildDeclRefExpr(
        HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue,
        Loc);
    ExprResult TypeCall =
        buildAlternativeTraitsCall(S, Loc, Traits, "type", HoldingRef);
    if (TypeCall.isInvalid())
      return true;
    if (!TypeCall.get()->getType()->isPointerType() &&
        !TypeCall.get()->isGLValue()) {
      S.Diag(Loc, diag::err_open_alternative_type_result) << SubjectType;
      return true;
    }
    VarDecl *TypeVar =
        BuildVarDecl(S, Loc, S.Context.getAutoRRefDeductType(), TypeCall.get());
    if (TypeVar->isInvalidDecl())
      return true;
    Projection->setIntermediateVar(TypeVar);
  }

  MatchPatternInfo &PatternInfo = State.get(Pattern);
  PatternInfo.Projection = Projection;
  PatternInfo.IsOpenAlternative = true;
  PatternInfo.OpenAlternativeType = RequestedType;
  PatternInfo.OpenAlternativeProjectableWildcard = IsProjectableWildcard;

  VarDecl *HoldingVar = Projection->getHoldingVar();
  ExprValueKind SubjectValueKind = Subject->getValueKind();
  Expr *HoldingRef = S.BuildDeclRefExpr(
      HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue, Loc);
  Expr *ForwardedRef = asValueKind(S, HoldingRef, SubjectValueKind);
  VarDecl *TypeVar = Projection->getIntermediateVar();
  QualType TypeResult = TypeVar->getType().getNonReferenceType();
  bool HasEmpty = TypeResult->isPointerType();
  PatternInfo.OpenAlternativeHasEmpty = HasEmpty;

  Expr *TypeRef = S.BuildDeclRefExpr(TypeVar, TypeResult, VK_LValue, Loc);
  ExprResult RawCondition;
  if (Pattern->isEmpty() || IsProjectableWildcard || HasEmpty) {
    if (!HasEmpty) {
      if (Pattern->isEmpty()) {
        S.Diag(Loc, diag::err_empty_alternative_not_found) << SubjectType;
        return true;
      }
      RawCondition = CXXBoolLiteralExpr::Create(S.Context, /*Value=*/true,
                                                S.Context.BoolTy, Loc);
    } else {
      RawCondition = S.PerformContextuallyConvertToBool(TypeRef);
      if (RawCondition.isInvalid())
        return true;
      if (Pattern->isEmpty())
        RawCondition = S.CreateBuiltinUnaryOp(Loc, UO_LNot, RawCondition.get());
    }
  }

  if (!RequestedType.isNull()) {
    ExprResult ActiveType = TypeRef;
    if (HasEmpty) {
      ActiveType = S.CreateBuiltinUnaryOp(Loc, UO_Deref, TypeRef);
      if (ActiveType.isInvalid())
        return true;
    }
    ExprResult RequestedTypeId = buildTypeId(S, Loc, RequestedType);
    if (RequestedTypeId.isInvalid())
      return true;
    ExprResult Equal =
        S.ActOnBinOp(S.getCurScope(), Loc, tok::TokenKind::equalequal,
                     ActiveType.get(), RequestedTypeId.get());
    if (Equal.isInvalid())
      return true;
    if (HasEmpty) {
      RawCondition = S.ActOnBinOp(S.getCurScope(), Loc, tok::TokenKind::ampamp,
                                  RawCondition.get(), Equal.get());
    } else {
      RawCondition = Equal;
    }
  }

  VarDecl *ConditionVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), RawCondition.get());
  if (ConditionVar->isInvalidDecl())
    return true;
  Projection->setConditionVar(ConditionVar);
  if (buildMatchProjectionCondition(S, Projection, Loc).isInvalid())
    return true;

  if (Pattern->isEmpty())
    return false;
  if (IsProjectableWildcard)
    return S.CheckCompleteMatchPattern(nullptr, SubPattern, State,
                                       ProjectionCache);

  if (auto *Type = dyn_cast<TypePattern>(SubPattern)) {
    MatchPatternInfo &TypeInfo = State.get(Type);
    TypeInfo.TypePatternResolved = true;
    TypeInfo.TypePatternMatches = true;
    TypeInfo.CheckedSubjectType = RequestedType;
    return false;
  }

  ExprResult GetCall = buildOpenAlternativeTraitsGetCall(
      S, Loc, Traits, SubjectType, RequestedType, ForwardedRef);
  if (GetCall.isInvalid())
    return true;
  Projection->setProjectedExpr(GetCall.get());
  return S.CheckCompleteMatchPattern(GetCall.get(), SubPattern, State,
                                     ProjectionCache);
}

static bool
checkBracedAlternativePattern(Sema &S, Expr *Subject,
                              AlternativePattern *Pattern,
                              Sema::MatchPatternState &State,
                              Sema::MatchProjectionCache *ProjectionCache) {
  SourceLocation Loc = Pattern->getBeginLoc();
  QualType SubjectType = Subject->getType().getNonReferenceType();
  AlternativeTraitsInfo Traits;
  if (lookupAlternativeTraits(S, Loc, SubjectType, Traits))
    return true;
  if (Traits.IsOpen)
    return checkOpenAlternativePattern(S, Subject, Pattern, Traits, State,
                                       ProjectionCache);

  llvm::SmallVector<unsigned, 4> Selected;
  if (Pattern->isNamed()) {
    std::optional<unsigned> Index =
        lookupAlternativeName(S, Pattern->getDiscriminatorRange().getBegin(),
                              SubjectType, Traits, Pattern->getName());
    if (!Index)
      return true;
    Selected.push_back(*Index);
  } else if (Pattern->isEmpty()) {
    for (unsigned I = 0; I < Traits.Size; ++I)
      if (!Traits.Projectable[I])
        Selected.push_back(I);
    if (Selected.empty()) {
      S.Diag(Loc, diag::err_empty_alternative_not_found) << SubjectType;
      return true;
    }
  } else {
    for (unsigned I = 0; I < Traits.Size; ++I)
      if (Traits.Projectable[I])
        Selected.push_back(I);
    if (Selected.empty()) {
      S.Diag(Loc, diag::err_braced_alternative_no_viable_state) << SubjectType;
      return true;
    }

    assert(ProjectionCache &&
           "generic alternative patterns require candidate specialization");
    if (!ProjectionCache)
      return true;

    unsigned Chosen = Selected.front();
    if (ProjectionCache->DeferAlternativeChoices) {
      ProjectionCache->AlternativeChoices.push_back({Selected, Chosen});
      ProjectionCache->HasDeferredAlternativeChoices = true;
      return S.CheckCompleteMatchPattern(nullptr, Pattern->getSubPattern(),
                                         State, ProjectionCache);
    }
    if (ProjectionCache->NextForcedAlternativeSelection <
        ProjectionCache->ForcedAlternativeSelections.size()) {
      Chosen = ProjectionCache->ForcedAlternativeSelections
                   [ProjectionCache->NextForcedAlternativeSelection++];
      if (!llvm::is_contained(Selected, Chosen))
        return true;
    }
    ProjectionCache->AlternativeChoices.push_back({Selected, Chosen});
    Selected.assign(1, Chosen);
  }

  if (!Pattern->isEmpty() && !Traits.Projectable[Selected.front()]) {
    S.Diag(Loc, diag::err_alternative_traits_member_missing)
        << SubjectType << "projectable selected state";
    return true;
  }

  QualType *AlternativeTypes =
      S.Context.Allocate<QualType>(Traits.Alternatives.size());
  std::uninitialized_copy(Traits.Alternatives.begin(),
                          Traits.Alternatives.end(), AlternativeTypes);
  auto *Projectable =
      S.Context.Allocate<unsigned char>(Traits.Projectable.size());
  llvm::transform(Traits.Projectable, Projectable,
                  [](bool Value) { return static_cast<unsigned char>(Value); });
  unsigned *SelectedAlternatives =
      S.Context.Allocate<unsigned>(Selected.size());
  std::uninitialized_copy(Selected.begin(), Selected.end(),
                          SelectedAlternatives);
  MatchPatternInfo &PatternInfo = State.get(Pattern);
  PatternInfo.AlternativeTypes =
      ArrayRef(AlternativeTypes, Traits.Alternatives.size());
  PatternInfo.ProjectableAlternatives =
      ArrayRef(Projectable, Traits.Projectable.size());
  PatternInfo.SelectedAlternatives =
      ArrayRef(SelectedAlternatives, Selected.size());
  PatternInfo.IsExhaustive = Traits.IsExhaustive;

  unsigned CacheKey = Pattern->isEmpty() ? 0 : Selected.front() + 1;
  if (MatchProjection *Projection = findMatchProjection(
          S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
          QualType(), CacheKey)) {
    PatternInfo.Projection = Projection;
    if (Pattern->isEmpty())
      return false;
    return S.CheckCompleteMatchPattern(Projection->getProjectedExpr(),
                                       Pattern->getSubPattern(), State,
                                       ProjectionCache);
  }

  MatchProjection *Projection = createMatchProjection(
      S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
      QualType(), CacheKey);
  PatternInfo.Projection = Projection;

  ExprValueKind SubjectValueKind = Subject->getValueKind();
  MatchProjection *SharedDiscriminator =
      findAlternativeDiscriminatorProjection(ProjectionCache, Subject);
  VarDecl *HoldingVar;
  if (SharedDiscriminator && SharedDiscriminator != Projection) {
    HoldingVar = SharedDiscriminator->getHoldingVar();
    Projection->setHoldingVar(HoldingVar);
    Projection->setIntermediateVar(SharedDiscriminator->getIntermediateVar());
  } else {
    HoldingVar =
        BuildVarDecl(S, Loc, S.Context.getAutoRRefDeductType(), Subject);
    if (HoldingVar->isInvalidDecl())
      return true;
    Projection->setHoldingVar(HoldingVar);
  }
  Expr *HoldingRef = S.BuildDeclRefExpr(
      HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue, Loc);
  Expr *ForwardedRef = asValueKind(S, HoldingRef, SubjectValueKind);

  VarDecl *IndexVar = Projection->getIntermediateVar();
  if (!IndexVar) {
    ExprResult IndexCall =
        buildAlternativeTraitsCall(S, Loc, Traits, "index", HoldingRef);
    if (IndexCall.isInvalid())
      return true;
    if (!IndexCall.get()->isInstantiationDependent() &&
        S.canThrow(IndexCall.get()) != CT_Cannot) {
      S.Diag(Loc, diag::err_alternative_traits_index_not_noexcept)
          << SubjectType;
      return true;
    }
    IndexVar =
        BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), IndexCall.get());
    if (IndexVar->isInvalidDecl())
      return true;
    Projection->setIntermediateVar(IndexVar);
  }
  Expr *IndexRef = S.BuildDeclRefExpr(
      IndexVar, IndexVar->getType().getNonReferenceType(), VK_LValue, Loc);

  ExprResult RawCondition;
  for (unsigned I : Selected) {
    ExprResult Target = S.ActOnIntegerConstant(Loc, I);
    if (Target.isInvalid())
      return true;
    ExprResult Equal =
        S.ActOnBinOp(S.getCurScope(), Loc, tok::TokenKind::equalequal, IndexRef,
                     Target.get());
    if (Equal.isInvalid())
      return true;
    RawCondition =
        RawCondition.isUnset()
            ? Equal
            : S.ActOnBinOp(S.getCurScope(), Loc, tok::TokenKind::pipepipe,
                           RawCondition.get(), Equal.get());
    if (RawCondition.isInvalid())
      return true;
  }

  VarDecl *ConditionVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), RawCondition.get());
  if (ConditionVar->isInvalidDecl())
    return true;
  Projection->setConditionVar(ConditionVar);
  if (buildMatchProjectionCondition(S, Projection, Loc).isInvalid())
    return true;

  if (Pattern->isEmpty())
    return false;

  unsigned Index = Selected.front();
  ExprResult GetCall =
      buildAlternativeTraitsCall(S, Loc, Traits, "get", ForwardedRef, Index);
  if (GetCall.isInvalid())
    return true;
  if (GetCall.get()->getType()->isVoidType()) {
    GetCall = S.ActOnFinishFullExpr(GetCall.get(), Loc,
                                    /*DiscardedValue=*/false);
    if (GetCall.isInvalid())
      return true;
    Projection->setProjectedExpr(GetCall.get());
    return S.CheckCompleteMatchPattern(GetCall.get(), Pattern->getSubPattern(),
                                       State, ProjectionCache);
  }
  ExprValueKind ProjectedValueKind = GetCall.get()->getValueKind();
  QualType ProjectedType = GetCall.get()->refersToBitField()
                               ? S.Context.getAutoDeductType()
                               : S.Context.getAutoRRefDeductType();
  VarDecl *ProjectedVar = BuildVarDecl(S, Loc, ProjectedType, GetCall.get());
  if (ProjectedVar->isInvalidDecl())
    return true;
  Projection->setProjectedVar(ProjectedVar);
  Expr *ProjectedRef = S.BuildDeclRefExpr(
      ProjectedVar, ProjectedVar->getType().getNonReferenceType(), VK_LValue,
      Loc);
  ProjectedRef = asValueKind(S, ProjectedRef, ProjectedValueKind);
  Projection->setProjectedExpr(ProjectedRef);
  return S.CheckCompleteMatchPattern(ProjectedRef, Pattern->getSubPattern(),
                                     State, ProjectionCache);
}

bool Sema::CheckCompleteMatchPatternImpl(
    Expr *Subject, MatchPattern *Pattern, MatchPatternState &State,
    MatchProjectionCache *ProjectionCache) {
  bool PatternIsInstantiationDependent = static_cast<bool>(
      Pattern->getDependence() & ExprDependence::Instantiation);
  if (Subject && PatternIsInstantiationDependent &&
      Pattern->getMatchPatternClass() ==
          MatchPattern::DeclarationPatternClass)
    return CheckCompleteMatchPattern(nullptr, Pattern, State);
  if (Subject && Subject->isTypeDependent() &&
      Pattern->getMatchPatternClass() !=
          MatchPattern::DeclarationPatternClass)
    return CheckCompleteMatchPattern(nullptr, Pattern, State);
  SourceLocation Loc = Pattern->getBeginLoc();
  Scope *S = getCurScope();
  switch (Pattern->getMatchPatternClass()) {
  case MatchPattern::WildcardPatternClass:
    break;
  case MatchPattern::ExpressionPatternClass: {
    // Subject is dependent.
    if (!Subject)
      return false;
    ExpressionPattern *P = static_cast<ExpressionPattern *>(Pattern);
    ExprResult Cond =
        ActOnBinOp(S, Loc, tok::TokenKind::equalequal, Subject, P->getExpr());
    if (Cond.isInvalid()) {
      return true;
    }
    State.get(P).Condition = Cond.get();
    break;
  }
  case MatchPattern::DeclarationPatternClass: {
    auto *P = static_cast<DeclarationPattern *>(Pattern);
    if (!Subject) {
      VarDecl *VD = P->getDeclaration();
      ParsingInitForAutoVars.erase(VD);
      if (auto *DD = dyn_cast<DecompositionDecl>(VD))
        for (BindingDecl *Binding : DD->bindings())
          ParsingInitForAutoVars.erase(Binding);
      if (VD->getType()->getContainedAutoType()) {
        VD->setType(SubstAutoTypeDependent(VD->getType()));
        VD->setTypeSourceInfo(
            SubstAutoTypeSourceInfoDependent(VD->getTypeSourceInfo()));
      }
      if (auto *DD = dyn_cast<DecompositionDecl>(VD))
        CheckCompleteDecompositionDeclaration(DD);
      return false;
    }

    VarDecl *VD = P->getDeclaration();
    auto *Decomposition = dyn_cast<DecompositionDecl>(VD);
    if (isReusableDecompositionDeclaration(VD)) {
      unsigned Arity = Decomposition->bindings().size();
      if (MatchProjection *Projection = findMatchProjection(
              *this, ProjectionCache, Subject,
              MatchProjection::DecompositionProjection, QualType(), Arity)) {
        State.get(P).Projection = Projection;
        DecompositionDecl *Canonical = Projection->getDecomposedDecl();
        for (auto [Alias, Binding] :
             llvm::zip(Decomposition->bindings(), Canonical->bindings())) {
          Expr *CanonicalRef = BuildDeclRefExpr(
              Binding, Binding->getType(), VK_LValue, Alias->getLocation());
          Alias->setBinding(Binding->getType(), CanonicalRef);
          Alias->setDecomposedDecl(Canonical);
        }
        FinalizeDeclaration(Decomposition);
        return false;
      }

      MatchProjection *Projection = createMatchProjection(
          *this, ProjectionCache, Subject,
          MatchProjection::DecompositionProjection, QualType(), Arity);
      State.get(P).Projection = Projection;
    }

    QualType PatternType = VD->getType();
    bool IsApplicable = true;
    if (!Subject->isTypeDependent() && PatternType->getContainedAutoType()) {
      IsApplicable =
          isDeducedDeclarationPatternApplicable(*this, VD, Subject) &&
          (!Decomposition || isDecompositionDeclarationPatternApplicable(
                                 *this, Decomposition, Subject));
    } else if (!Subject->isTypeDependent()) {
      if (!isExactDeclarationPatternMatch(*this, Subject, PatternType)) {
        switch (buildDeclarationLikeCastProjection(
            *this, Subject, P, PatternType, State, ProjectionCache)) {
        case CastProjectionResult::Success:
          Subject = State.get(P).Projection->getProjectedExpr();
          break;
        case CastProjectionResult::NotApplicable:
          Diag(P->getBeginLoc(), diag::err_declaration_pattern_not_exact_match)
              << PatternType << Subject->getType();
          VD->setInvalidDecl();
          return true;
        case CastProjectionResult::Error:
          VD->setInvalidDecl();
          return true;
        }
      }
    }

    auto Initialize = [&] {
      AddInitializerToDecl(VD, Subject, /*DirectInit=*/false);
      FinalizeDeclaration(VD);
      return hasFailedVariableInitialization(VD);
    };
    if (IsApplicable) {
      // Candidate omission is analogous to overload candidate viability. Once
      // the declaration selector applies, ordinary initialization failures do
      // not cause matching to retry a later arm.
      NonSFINAEContext NonSFINAE(*this);
      if (Initialize())
        return true;
    } else if (Initialize()) {
      return true;
    }
    if (MatchProjection *Projection = State.get(P).Projection)
      Projection->setDecomposedDecl(Decomposition);
    break;
  }
  case MatchPattern::TypePatternClass: {
    auto *P = static_cast<TypePattern *>(Pattern);
    if (!Subject || Subject->isTypeDependent() ||
        P->getType()->isDependentType())
      return false;

    QualType PatternType = P->getType();
    MatchPatternInfo &Info = State.get(P);
    if (Subject->getType()->isVoidType()) {
      if (!PatternType->isVoidType()) {
        Diag(P->getBeginLoc(), diag::err_type_pattern_not_exact_match)
            << PatternType << Subject->getType();
        return true;
      }
      Info.TypePatternResolved = true;
      Info.TypePatternMatches = true;
      Info.CheckedSubjectType = Subject->getType();
      return false;
    }

    if (isExactDeclarationPatternMatch(*this, Subject, PatternType)) {
      NonSFINAEContext NonSFINAE(*this);
      if (hasFailedVariableInitialization(
              BuildVarDecl(*this, P->getBeginLoc(), PatternType, Subject)))
        return true;
      Info.TypePatternResolved = true;
      Info.TypePatternMatches = true;
      Info.CheckedSubjectType = Subject->getType();
      return false;
    }

    switch (buildDeclarationLikeCastProjection(*this, Subject, P, PatternType,
                                               State, ProjectionCache)) {
    case CastProjectionResult::Success:
      if (PatternType->isVoidType()) {
        Info.TypePatternResolved = true;
        Info.TypePatternMatches = true;
        Info.CheckedSubjectType = Subject->getType();
        return false;
      }
      if (isExactDeclarationPatternMatch(
              *this, Info.Projection->getProjectedExpr(), PatternType)) {
        NonSFINAEContext NonSFINAE(*this);
        if (hasFailedVariableInitialization(
                BuildVarDecl(*this, P->getBeginLoc(), PatternType,
                             Info.Projection->getProjectedExpr())))
          return true;
        Info.TypePatternResolved = true;
        Info.TypePatternMatches = true;
        Info.CheckedSubjectType = Subject->getType();
        return false;
      }
      Info.Projection = nullptr;
      [[fallthrough]];
    case CastProjectionResult::NotApplicable:
      Diag(P->getBeginLoc(), diag::err_type_pattern_not_exact_match)
          << PatternType << Subject->getType();
      return true;
    case CastProjectionResult::Error:
      return true;
    }
    break;
  }
  case MatchPattern::AlternativePatternClass: {
    AlternativePattern *P = static_cast<AlternativePattern *>(Pattern);
    if (!Subject) {
      if (P->isEmpty())
        return false;
      return CheckCompleteMatchPattern(nullptr, P->getSubPattern(), State);
    }
    return checkBracedAlternativePattern(*this, Subject, P, State,
                                         ProjectionCache);
  }
  case MatchPattern::DecompositionPatternClass: {
    DecompositionPattern *P = static_cast<DecompositionPattern *>(Pattern);
    size_t SavedProjectionPathSize =
        ProjectionCache ? ProjectionCache->CurrentProjectionPath.size() : 0;
    llvm::scope_exit RestoreProjectionPath([&] {
      if (ProjectionCache)
        ProjectionCache->CurrentProjectionPath.resize(SavedProjectionPathSize);
    });
    if (!Subject) {
      for (MatchPattern *C : P->children()) {
        if (CheckCompleteMatchPattern(nullptr, C, State))
          return true;
      }
      return false;
    }
    if (MatchProjection *Projection =
            findMatchProjection(*this, ProjectionCache, Subject,
                                MatchProjection::DecompositionProjection,
                                QualType(), P->getNumPatterns())) {
      DecompositionDecl *Decomposed = Projection->getDecomposedDecl();
      State.get(P).Projection = Projection;
      for (auto [Binding, Child] :
           llvm::zip(Decomposed->bindings(), P->children())) {
        if (CheckCompleteMatchPattern(Binding->getBinding(), Child,
                                      State, ProjectionCache))
          return true;
        if (ProjectionCache)
          appendProjectionPath(Child, ProjectionCache->CurrentProjectionPath,
                               State);
      }
      return false;
    }
    MatchProjection *Projection =
        createMatchProjection(*this, ProjectionCache, Subject,
                              MatchProjection::DecompositionProjection,
                              QualType(), P->getNumPatterns());
    State.get(P).Projection = Projection;
    QualType Type = Context.getAutoRRefDeductType();
    TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(Type, Loc);
    SmallVector<BindingDecl *, 8> Bindings;
    Bindings.reserve(P->getNumPatterns());
    for (MatchPattern *C : P->children()) {
      BindingDecl *BD = BindingDecl::Create(
          Context, CurContext, C->getBeginLoc(), nullptr, QualType());
      BD->setImplicit();
      Bindings.push_back(BD);
    }
    DecompositionDecl *Decomposed = DecompositionDecl::Create(
        Context, CurContext, Loc, Loc, Loc, Type, TInfo, SC_None, Bindings);
    Projection->setDecomposedDecl(Decomposed);
    Decomposed->setImplicit();
    // TODO: Consider ActOnInitializerError
    AddInitializerToDecl(Decomposed, Subject, /*DirectInit=*/false);
    if (Decomposed->isInvalidDecl()) {
      return true;
    }
    unsigned I = 0;
    for (MatchPattern *C : P->children()) {
      BindingDecl *BD = Bindings[I];
      if (CheckCompleteMatchPattern(BD->getBinding(), C, State,
                                    ProjectionCache)) {
        return true;
      }
      if (ProjectionCache)
        appendProjectionPath(C, ProjectionCache->CurrentProjectionPath, State);
      ++I;
    }
    break;
  }
  }
  return false;
}

bool Sema::CheckCompleteMatchPattern(Expr *Subject, MatchPattern *Pattern,
                                     MatchPatternState &State,
                                     MatchProjectionCache *ProjectionCache) {
  State.get(Pattern);
  return CheckCompleteMatchPatternImpl(Subject, Pattern, State,
                                       ProjectionCache);
}

Sema::MatchPatternSemanticAnalysis
Sema::AnalyzeMatchPatternSemantics(MatchPattern *Pattern,
                                   const MatchPatternState &State) {
  MatchPatternSemanticAnalysis Result;
  auto Analyze = [&](MatchPattern *P,
                     auto &Recurse) -> MatchPatternRefutability {
    const MatchPatternInfo *Info = State.find(P);
    switch (P->getMatchPatternClass()) {
    case MatchPattern::WildcardPatternClass:
      return MatchPatternRefutability::Irrefutable;
    case MatchPattern::ExpressionPatternClass:
    case MatchPattern::AlternativePatternClass:
      return MatchPatternRefutability::Refutable;
    case MatchPattern::DeclarationPatternClass:
      return Info && Info->Projection &&
                     Info->Projection->getKind() == MatchProjection::CastProjection
                 ? MatchPatternRefutability::Refutable
                 : MatchPatternRefutability::Irrefutable;
    case MatchPattern::TypePatternClass:
      if (!Info || !Info->TypePatternResolved)
        return MatchPatternRefutability::Refutable;
      if (!Info->TypePatternMatches)
        return MatchPatternRefutability::Impossible;
      return Info->Projection &&
                     Info->Projection->getKind() == MatchProjection::CastProjection
                 ? MatchPatternRefutability::Refutable
                 : MatchPatternRefutability::Irrefutable;
    case MatchPattern::ParenPatternClass:
      return Recurse(static_cast<ParenPattern *>(P)->getSubPattern(), Recurse);
    case MatchPattern::DecompositionPatternClass: {
      MatchPatternRefutability Refutability =
          MatchPatternRefutability::Irrefutable;
      for (MatchPattern *Child : P->children()) {
        MatchPatternRefutability ChildResult = Recurse(Child, Recurse);
        if (ChildResult == MatchPatternRefutability::Impossible)
          return ChildResult;
        if (ChildResult == MatchPatternRefutability::Refutable)
          Refutability = ChildResult;
      }
      return Refutability;
    }
    }
    llvm_unreachable("unknown match pattern kind");
  };
  Result.Refutability = Analyze(Pattern, Analyze);
  return Result;
}
