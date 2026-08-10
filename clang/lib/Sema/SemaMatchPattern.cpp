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
#include "TreeTransform.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/MatchPattern.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/EnterExpressionEvaluationContext.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/SemaObjC.h"
#include "clang/Sema/TemplateDeduction.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace clang;
using namespace sema;

static void
collectPatternBindings(MatchPattern *Pattern,
                       llvm::SmallPtrSetImpl<const ValueDecl *> &Bindings) {
  if (auto *Declaration = dyn_cast<DeclarationPattern>(Pattern)) {
    VarDecl *Variable = Declaration->getDeclaration();
    Bindings.insert(Variable);
    if (auto *Decomposition = dyn_cast<DecompositionDecl>(Variable))
      for (BindingDecl *Binding : Decomposition->bindings())
        Bindings.insert(Binding);
  }

  for (MatchPattern *Child : Pattern->children())
    collectPatternBindings(Child, Bindings);
}

static bool checkPatternBindingReferences(
    Sema &S, MatchPattern *Pattern,
    const llvm::SmallPtrSetImpl<const ValueDecl *> &Bindings) {
  bool Invalid = false;
  if (auto *Expression = dyn_cast<ExpressionPattern>(Pattern)) {
    SmallVector<Stmt *, 16> Worklist{Expression->getExpr()};
    while (!Worklist.empty()) {
      Stmt *Node = Worklist.pop_back_val();
      if (auto *Reference = dyn_cast<DeclRefExpr>(Node)) {
        if (Bindings.contains(Reference->getDecl())) {
          S.Diag(Reference->getExprLoc(),
                 diag::err_pattern_binding_used_in_pattern)
              << Reference->getDecl();
          Invalid = true;
        }
      }
      for (Stmt *Child : Node->children())
        if (Child)
          Worklist.push_back(Child);
    }
  }

  for (MatchPattern *Child : Pattern->children())
    Invalid |= checkPatternBindingReferences(S, Child, Bindings);
  return Invalid;
}

static bool checkPatternBindingReferences(Sema &S, MatchPattern *Pattern) {
  llvm::SmallPtrSet<const ValueDecl *, 8> Bindings;
  collectPatternBindings(Pattern, Bindings);
  return checkPatternBindingReferences(S, Pattern, Bindings);
}

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
  bool OpenHasValue = false;
  llvm::SmallVector<QualType, 4> Alternatives;
  llvm::SmallVector<bool, 4> Projectable;
};

struct NamedAlternativeInfo {
  unsigned Index;
  AlternativeTraitsInfo Traits;
};
} // namespace

static bool initializeAlternativeTraitsInfo(
    Sema &S, SourceLocation Loc, QualType SubjectType, QualType TraitType,
    LookupResult &SizeLookup, bool AllowOpen, AlternativeTraitsInfo &Info) {
  Info.Type = TraitType;
  Info.Record = TraitType->getAsCXXRecordDecl();
  if (!Info.Record)
    return true;

  if (SizeLookup.empty()) {
    LookupResult TryCastLookup(S, S.PP.getIdentifierInfo("try_cast"), Loc,
                               Sema::LookupOrdinaryName);
    S.LookupQualifiedName(TryCastLookup, Info.Record);
    if (!AllowOpen || TryCastLookup.empty()) {
      S.Diag(Loc, diag::err_alternative_traits_member_missing)
          << SubjectType
          << (AllowOpen ? "either 'size' or 'try_cast'" : "'size'");
      return true;
    }
    LookupResult HasValueLookup(S, S.PP.getIdentifierInfo("has_value"), Loc,
                                Sema::LookupOrdinaryName);
    S.LookupQualifiedName(HasValueLookup, Info.Record);
    Info.IsOpen = true;
    Info.IsExhaustive = false;
    Info.OpenHasValue = !HasValueLookup.empty();
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
  return false;
}

static bool lookupAlternativeTraits(Sema &S, SourceLocation Loc,
                                    QualType SubjectType,
                                    AlternativeTraitsInfo &Info) {
  SubjectType = SubjectType.getNonReferenceType().getUnqualifiedType();
  if (const auto *Pointer = SubjectType->getAs<PointerType>()) {
    if (Pointer->getPointeeType()->isVoidType()) {
      S.Diag(Loc, diag::err_braced_alternative_void_pointer);
      return true;
    }
    Info.Type = SubjectType;
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
  return initializeAlternativeTraitsInfo(S, Loc, SubjectType, Info.Type,
                                         SizeLookup, /*AllowOpen=*/true, Info);
}

static std::optional<NamedAlternativeInfo>
lookupAlternativeName(Sema &S, SourceLocation Loc, QualType SubjectType,
                      const AlternativeTraitsInfo &Info, IdentifierInfo *Name) {
  if (Info.IsBuiltinPointer) {
    unsigned Index;
    if (Name->isStr("none"))
      Index = 0;
    else if (Name->isStr("some"))
      Index = 1;
    else {
      S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
      return std::nullopt;
    }
    return NamedAlternativeInfo{Index, Info};
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

  QualType NameType =
      NameDecl->getType().getNonReferenceType().getUnqualifiedType();
  auto *NameSpecialization = dyn_cast_or_null<ClassTemplateSpecializationDecl>(
      NameType->getAsCXXRecordDecl());
  if (!NameSpecialization || !NameSpecialization->isInStdNamespace() ||
      !NameSpecialization->getIdentifier() ||
      NameSpecialization->getName() != "alternative_name" ||
      NameSpecialization->getTemplateArgs().size() != 1 ||
      NameSpecialization->getTemplateArgs()[0].getKind() !=
          TemplateArgument::Type) {
    S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
    return std::nullopt;
  }

  Expr *NameExpr =
      S.BuildDeclRefExpr(NameDecl, NameDecl->getType(), VK_LValue, Loc);
  CXXScopeSpec SS;
  DeclarationNameInfo IndexName(S.PP.getIdentifierInfo("index"), Loc);
  ExprResult IndexExpr = S.BuildMemberReferenceExpr(
      NameExpr, NameExpr->getType(), Loc, /*IsArrow=*/false, SS,
      SourceLocation(), nullptr, IndexName, /*TemplateArgs=*/nullptr,
      /*Scope=*/nullptr);
  if (IndexExpr.isInvalid())
    return std::nullopt;
  llvm::APSInt Value(32);
  if (S.VerifyIntegerConstantExpression(IndexExpr.get(), &Value).isInvalid())
    return std::nullopt;

  QualType ProviderType = NameSpecialization->getTemplateArgs()[0].getAsType();
  if (S.RequireCompleteType(Loc, ProviderType,
                            diag::err_alternative_traits_member_missing,
                            SubjectType, "complete alternative provider"))
    return std::nullopt;
  CXXRecordDecl *ProviderRecord = ProviderType->getAsCXXRecordDecl();
  if (!ProviderRecord)
    return std::nullopt;
  LookupResult SizeLookup(S, S.PP.getIdentifierInfo("size"), Loc,
                          Sema::LookupOrdinaryName);
  S.LookupQualifiedName(SizeLookup, ProviderRecord);
  AlternativeTraitsInfo ProviderInfo;
  if (initializeAlternativeTraitsInfo(S, Loc, SubjectType, ProviderType,
                                      SizeLookup, /*AllowOpen=*/false,
                                      ProviderInfo))
    return std::nullopt;

  unsigned Index = Value.getLimitedValue(UINT_MAX);
  if (Index >= ProviderInfo.Size) {
    S.Diag(Loc, diag::err_alternative_name_not_found) << Name << SubjectType;
    return std::nullopt;
  }
  return NamedAlternativeInfo{Index, std::move(ProviderInfo)};
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

static void determineAlternativeProjections(Sema &S, SourceLocation Loc,
                                            Expr *Subject,
                                            AlternativeTraitsInfo &Info) {
  if (Info.IsBuiltinPointer)
    return;

  Info.Alternatives.reserve(Info.Size);
  Info.Projectable.reserve(Info.Size);
  for (unsigned I = 0; I < Info.Size; ++I) {
    auto *Probe = new (S.Context)
        OpaqueValueExpr(Loc, Subject->getType(), Subject->getValueKind(),
                        Subject->getObjectKind(), Subject);
    EnterExpressionEvaluationContext Unevaluated(
        S, Sema::ExpressionEvaluationContext::Unevaluated);
    Sema::SFINAETrap Trap(S, /*ForValidityCheck=*/true);
    ExprResult GetCall =
        buildAlternativeTraitsCall(S, Loc, Info, "get", Probe, I);
    bool IsProjectable = GetCall.isUsable() && !Trap.hasErrorOccurred();
    Info.Alternatives.push_back(IsProjectable ? GetCall.get()->getType()
                                              : S.Context.VoidTy);
    Info.Projectable.push_back(IsProjectable);
  }
}

static ExprResult buildOpenAlternativeTraitsTryCastCall(
    Sema &S, SourceLocation Loc, const AlternativeTraitsInfo &Info,
    QualType SubjectType, QualType RequestedType, Expr *Subject) {
  LookupResult TryCastLookup(S, S.PP.getIdentifierInfo("try_cast"), Loc,
                             Sema::LookupOrdinaryName);
  S.LookupQualifiedName(TryCastLookup, Info.Record);
  if (TryCastLookup.empty()) {
    S.Diag(Loc, diag::err_alternative_traits_member_missing)
        << SubjectType << "'try_cast'";
    return ExprError();
  }

  CXXScopeSpec SS;
  SS.MakeTrivial(S.Context, NestedNameSpecifier(Info.Type.getTypePtr()), Loc);
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(getTrivialTypeTemplateArgument(S, Loc, RequestedType));
  ExprResult Callee = S.BuildTemplateIdExpr(SS, SourceLocation(), TryCastLookup,
                                            /*RequiresADL=*/false, &Args);
  if (Callee.isInvalid())
    return ExprError();
  return S.BuildCallExpr(nullptr, Callee.get(), Loc, Subject, Loc);
}

namespace {
class MatchSubjectDefaultArgFinder
    : public EvaluatedExprVisitor<MatchSubjectDefaultArgFinder> {
  using Inherited = EvaluatedExprVisitor<MatchSubjectDefaultArgFinder>;

public:
  bool ContainsDefaultArg = false;
  bool NeedsRebuild = false;

  explicit MatchSubjectDefaultArgFinder(ASTContext &Context)
      : Inherited(Context) {}

  void VisitCXXDefaultArgExpr(CXXDefaultArgExpr *E) {
    ContainsDefaultArg = true;
    NeedsRebuild |= !E->hasRewrittenInit();
  }
};

class MatchSubjectLifetimeRebuilder
    : public TreeTransform<MatchSubjectLifetimeRebuilder> {
public:
  explicit MatchSubjectLifetimeRebuilder(Sema &S) : TreeTransform(S) {}

  bool AlreadyTransformed(QualType T) {
    return T.isNull() || !T->isInstantiationDependentType();
  }

  ExprResult TransformLambdaExpr(LambdaExpr *E) { return E; }
  ExprResult TransformBlockExpr(BlockExpr *E) { return E; }
  ExprResult TransformOpaqueValueExpr(OpaqueValueExpr *E) { return E; }

  bool ReplacingOriginal() { return true; }
  bool AllowSkippingCXXConstructExpr() { return false; }
};
} // namespace

static ExprResult rebuildMatchSubjectForLifetimeExtension(
    Sema &S, Expr *Subject,
    SmallVectorImpl<MaterializeTemporaryExpr *> &Temporaries,
    bool &ContainsDefaultArg) {
  MatchSubjectDefaultArgFinder Finder(S.Context);
  Finder.Visit(Subject);
  ContainsDefaultArg = Finder.ContainsDefaultArg;
  if (!Finder.NeedsRebuild)
    return Subject;

  EnterExpressionEvaluationContext LifetimeContext(
      S, Sema::ExpressionEvaluationContext::PotentiallyEvaluated,
      /*LambdaContextDecl=*/nullptr,
      Sema::ExpressionEvaluationContextRecord::EK_Other,
      /*ShouldEnter=*/true);
  auto &Record = S.currentEvaluationContext();
  Record.InLifetimeExtendingContext = true;
  Record.RebuildDefaultArgOrDefaultInit = true;

  ExprResult Rebuilt = MatchSubjectLifetimeRebuilder(S).TransformExpr(Subject);
  Temporaries = std::move(Record.ForRangeLifetimeExtendTemps);
  return Rebuilt;
}

ExprResult Sema::ActOnMatchSubject(Expr *Subject, VarDecl *&HoldingVar) {
  SmallVector<MaterializeTemporaryExpr *, 8> RebuiltTemporaries;
  bool ContainsDefaultArg = false;
  ExprResult Rebuilt = rebuildMatchSubjectForLifetimeExtension(
      *this, Subject, RebuiltTemporaries, ContainsDefaultArg);
  if (Rebuilt.isInvalid())
    return ExprError();
  Subject = Rebuilt.get();

  if (Subject->getType()->isVoidType())
    return Subject;

  bool IsConstant = Subject->isCXX11ConstantExpr(Context);
  bool MaterializeConstantPrValue =
      IsConstant && Subject->isPRValue() && Subject->getType()->isRecordType();
  if (IsConstant && !MaterializeConstantPrValue && !ContainsDefaultArg)
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

  // Associate every materialized temporary in the subject with its hidden
  // holder. In an expression match the holder ends with the full-expression;
  // in an unparenthesized condition it has condition-variable lifetime.
  class TemporaryCollector : public EvaluatedExprVisitor<TemporaryCollector> {
    using Inherited = EvaluatedExprVisitor<TemporaryCollector>;
    SmallVectorImpl<MaterializeTemporaryExpr *> &Temporaries;

  public:
    TemporaryCollector(ASTContext &Context,
                       SmallVectorImpl<MaterializeTemporaryExpr *> &Temporaries)
        : Inherited(Context), Temporaries(Temporaries) {}

    void VisitMaterializeTemporaryExpr(MaterializeTemporaryExpr *E) {
      if (!E->getExtendingDecl())
        Temporaries.push_back(E);
      Inherited::VisitStmt(E);
    }

    void VisitCXXDefaultArgExpr(CXXDefaultArgExpr *E) {
      if (E->hasRewrittenInit())
        Visit(E->getRewrittenExpr());
    }
  };

  SmallVector<MaterializeTemporaryExpr *, 8> Temporaries =
      std::move(RebuiltTemporaries);
  TemporaryCollector(Context, Temporaries).Visit(HoldingVar->getInit());
  llvm::SmallPtrSet<MaterializeTemporaryExpr *, 8> Seen;
  llvm::erase_if(Temporaries, [&](MaterializeTemporaryExpr *Temporary) {
    return Temporary->getExtendingDecl() || !Seen.insert(Temporary).second;
  });
  ApplyForRangeOrExpansionStatementLifetimeExtension(HoldingVar, Temporaries);

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

static bool isNonTriviallyMoveInitialized(const VarDecl *Declaration) {
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
  return Construct && Construct->getConstructor()->isMoveConstructor() &&
         !Construct->getConstructor()->isTrivial();
}

} // namespace

void Sema::CheckGuardedMatchPattern(MatchPattern *Pattern) {
  forEachDeclarationPattern(Pattern, [&](DeclarationPattern *P) {
    VarDecl *Declaration = P->getDeclaration();
    if (isNonTriviallyMoveInitialized(Declaration))
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
  auto Entity = InitializedEntity::InitializeMatchExprResult(Loc, RetTy);
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
    bool NeedsCaseInstantiation, bool CaseConditionSyntax,
    ArrayRef<MatchTestInstantiation> Instantiations) {
  return new (Context) MatchTestExpr(
      Context, HoldingVar, Subject, MatchLoc, Pattern, Instantiation, IfLoc,
      Guard, PatternIsIrrefutable, NeedsCaseInstantiation, CaseConditionSyntax,
      Instantiations);
}

ExprResult Sema::ActOnMatchSelectExpr(
    VarDecl *HoldingVar, Expr *Subject, SourceLocation MatchLoc,
    bool IsConstexpr, TypeLoc OrigResultType, QualType RetTy,
    SmallVectorImpl<MatchCase> &SourceCases, SourceRange Braces,
    bool ExpandDeferredCases, bool RequireFirstCaseViable,
    std::optional<ArrayRef<MatchCaseInstantiation>> Instantiations,
    std::optional<ArrayRef<MatchCaseInstantiation>> DiagnosticInstantiations) {
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

  CheckMatchSelectExhaustiveness(
      Subject, SourceCases,
      DiagnosticInstantiations.value_or(
          ArrayRef<MatchCaseInstantiation>(CaseInstantiations)));
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
findAlternativeHoldingProjection(Sema::MatchProjectionCache *Cache,
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

static MatchProjection *findAlternativeDiscriminatorProjection(
    Sema &S, Sema::MatchProjectionCache *Cache, const Expr *Subject,
    QualType ProviderType) {
  if (!Cache)
    return nullptr;
  for (const Sema::MatchProjectionCache::Entry &Entry : Cache->Entries) {
    if (Entry.Subject == Subject &&
        Entry.Projection->getKind() == MatchProjection::AlternativeProjection &&
        Entry.Path == Cache->CurrentProjectionPath &&
        !Entry.Discriminator.isNull() &&
        S.Context.hasSameType(Entry.Discriminator, ProviderType))
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

static Expr *getDecompositionElement(Sema &S, Expr *Subject,
                                     BindingDecl *Binding) {
  ExprValueKind ValueKind = Subject->isLValue() ? VK_LValue : VK_XValue;
  if (VarDecl *HoldingVar = Binding->getHoldingVar())
    ValueKind = HoldingVar->getType()->isLValueReferenceType() ? VK_LValue
                                                               : VK_XValue;
  return asValueKind(S, Binding->getBinding(), ValueKind);
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
  bool ProjectsPointer = false;
  if (!isDynamicCastDeclaration(S, Loc, SubjectType, TargetType,
                                ProjectsPointer))
    return CastProjectionResult::NotApplicable;

  bool DereferenceResult = !ProjectsPointer;
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
  }

  TypeSourceInfo *TSI =
      S.Context.getTrivialTypeSourceInfo(DynamicTargetType, Loc);
  ExprResult CastExpr =
      S.BuildCXXNamedCast({}, tok::kw_dynamic_cast, TSI, CastOperand, {}, {});
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
  ExprValueKind ProjectedValueKind =
      DereferenceResult ? SubjectValueKind : CastExpr.get()->getValueKind();
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

  MatchPatternInfo &PatternInfo = State.get(Pattern);
  PatternInfo.IsOpenAlternative = true;
  PatternInfo.OpenAlternativeType = RequestedType;
  PatternInfo.OpenAlternativeProjectableWildcard = IsProjectableWildcard;
  PatternInfo.OpenAlternativeHasEmpty = Traits.OpenHasValue;

  ExprValueKind SubjectValueKind = Subject->getValueKind();
  auto GetHoldingVar = [&]() -> VarDecl * {
    if (MatchProjection *Shared =
            findAlternativeHoldingProjection(ProjectionCache, Subject))
      if (Shared->getHoldingVar())
        return Shared->getHoldingVar();
    return BuildVarDecl(S, Loc, S.Context.getAutoRRefDeductType(), Subject);
  };

  if (RequestedType.isNull()) {
    if (Pattern->isEmpty() && !Traits.OpenHasValue) {
      S.Diag(Loc, diag::err_empty_alternative_not_found) << SubjectType;
      return true;
    }

    constexpr unsigned OpenStateCacheKey = ~0u;
    MatchProjection *StateProjection = findMatchProjection(
        S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
        Traits.Type, OpenStateCacheKey);
    if (!StateProjection) {
      StateProjection = createMatchProjection(
          S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
          Traits.Type, OpenStateCacheKey);
      VarDecl *HoldingVar = GetHoldingVar();
      if (HoldingVar->isInvalidDecl())
        return true;
      StateProjection->setHoldingVar(HoldingVar);

      if (Traits.OpenHasValue) {
        Expr *HoldingRef = S.BuildDeclRefExpr(
            HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue,
            Loc);
        ExprResult HasValueCall =
            buildAlternativeTraitsCall(S, Loc, Traits, "has_value", HoldingRef);
        if (HasValueCall.isInvalid())
          return true;
        VarDecl *HasValueVar = BuildVarDecl(
            S, Loc, S.Context.getAutoDeductType(), HasValueCall.get());
        if (HasValueVar->isInvalidDecl())
          return true;
        StateProjection->setIntermediateVar(HasValueVar);
      }
    }

    MatchProjection *Projection = createMatchProjection(
        S, /*Cache=*/nullptr, Subject, MatchProjection::AlternativeProjection);
    Projection->setHoldingVar(StateProjection->getHoldingVar());
    Projection->setIntermediateVar(StateProjection->getIntermediateVar());
    PatternInfo.Projection = Projection;

    ExprResult RawCondition;
    if (!Traits.OpenHasValue) {
      RawCondition = CXXBoolLiteralExpr::Create(S.Context, /*Value=*/true,
                                                S.Context.BoolTy, Loc);
    } else {
      VarDecl *HasValueVar = Projection->getIntermediateVar();
      Expr *HasValueRef = S.BuildDeclRefExpr(
          HasValueVar, HasValueVar->getType().getNonReferenceType(), VK_LValue,
          Loc);
      RawCondition = S.PerformContextuallyConvertToBool(HasValueRef);
      if (RawCondition.isInvalid())
        return true;
      if (Pattern->isEmpty())
        RawCondition = S.CreateBuiltinUnaryOp(Loc, UO_LNot, RawCondition.get());
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
    return S.CheckCompleteMatchPattern(nullptr, SubPattern, State,
                                       ProjectionCache);
  }

  QualType CastType = RequestedType.getNonReferenceType().getUnqualifiedType();
  if (MatchProjection *Projection = findMatchProjection(
          S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
          CastType)) {
    PatternInfo.Projection = Projection;
    return S.CheckCompleteMatchPattern(Projection->getProjectedExpr(),
                                       SubPattern, State, ProjectionCache);
  }

  MatchProjection *Projection =
      createMatchProjection(S, ProjectionCache, Subject,
                            MatchProjection::AlternativeProjection, CastType);
  PatternInfo.Projection = Projection;
  VarDecl *HoldingVar = GetHoldingVar();
  if (HoldingVar->isInvalidDecl())
    return true;
  Projection->setHoldingVar(HoldingVar);
  Expr *HoldingRef = S.BuildDeclRefExpr(
      HoldingVar, HoldingVar->getType().getNonReferenceType(), VK_LValue, Loc);
  Expr *ForwardedRef = asValueKind(S, HoldingRef, SubjectValueKind);

  ExprResult TryCastCall = buildOpenAlternativeTraitsTryCastCall(
      S, Loc, Traits, SubjectType, CastType, ForwardedRef);
  if (TryCastCall.isInvalid())
    return true;
  if (!TryCastCall.get()->isTypeDependent() &&
      !TryCastCall.get()->getType()->isPointerType()) {
    S.Diag(Loc, diag::err_open_alternative_try_cast_result) << SubjectType;
    return true;
  }

  VarDecl *CastVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), TryCastCall.get());
  if (CastVar->isInvalidDecl())
    return true;
  Projection->setIntermediateVar(CastVar);
  Expr *CastRef = S.BuildDeclRefExpr(
      CastVar, CastVar->getType().getNonReferenceType(), VK_LValue, Loc);

  ExprResult RawCondition = S.CheckBooleanCondition(Loc, CastRef);
  if (RawCondition.isInvalid())
    return true;
  VarDecl *ConditionVar =
      BuildVarDecl(S, Loc, S.Context.getAutoDeductType(), RawCondition.get());
  if (ConditionVar->isInvalidDecl())
    return true;
  Projection->setConditionVar(ConditionVar);
  if (buildMatchProjectionCondition(S, Projection, Loc).isInvalid())
    return true;

  ExprResult Projected =
      S.ActOnUnaryOp(S.getCurScope(), Loc, tok::TokenKind::star, CastRef);
  if (Projected.isInvalid())
    return true;
  ExprValueKind ProjectedValueKind =
      SubjectValueKind == VK_LValue ? VK_LValue : VK_XValue;
  Expr *ProjectedExpr = asValueKind(S, Projected.get(), ProjectedValueKind);
  QualType ProjectedType = ProjectedExpr->refersToBitField()
                               ? S.Context.getAutoDeductType()
                               : S.Context.getAutoRRefDeductType();
  VarDecl *ProjectedVar = BuildVarDecl(S, Loc, ProjectedType, ProjectedExpr);
  if (ProjectedVar->isInvalidDecl())
    return true;
  Projection->setProjectedVar(ProjectedVar);
  Expr *ProjectedRef = S.BuildDeclRefExpr(
      ProjectedVar, ProjectedVar->getType().getNonReferenceType(), VK_LValue,
      Loc);
  ProjectedRef = asValueKind(S, ProjectedRef, ProjectedValueKind);
  Projection->setProjectedExpr(ProjectedRef);
  return S.CheckCompleteMatchPattern(ProjectedRef, SubPattern, State,
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
    std::optional<NamedAlternativeInfo> Named =
        lookupAlternativeName(S, Pattern->getDiscriminatorRange().getBegin(),
                              SubjectType, Traits, Pattern->getName());
    if (!Named)
      return true;
    Selected.push_back(Named->Index);
    Traits = std::move(Named->Traits);
  }

  determineAlternativeProjections(S, Loc, Subject, Traits);

  if (Pattern->isEmpty()) {
    for (unsigned I = 0; I < Traits.Size; ++I)
      if (!Traits.Projectable[I])
        Selected.push_back(I);
    if (Selected.empty()) {
      S.Diag(Loc, diag::err_empty_alternative_not_found) << SubjectType;
      return true;
    }
  } else if (!Pattern->isNamed()) {
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

  if (Pattern->getSubPattern() && !Traits.Projectable[Selected.front()]) {
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
  PatternInfo.AlternativeProviderType = Traits.Type;
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
          Traits.Type, CacheKey)) {
    PatternInfo.Projection = Projection;
    if (!Pattern->getSubPattern())
      return false;
    return S.CheckCompleteMatchPattern(Projection->getProjectedExpr(),
                                       Pattern->getSubPattern(), State,
                                       ProjectionCache);
  }

  MatchProjection *Projection = createMatchProjection(
      S, ProjectionCache, Subject, MatchProjection::AlternativeProjection,
      Traits.Type, CacheKey);
  PatternInfo.Projection = Projection;

  ExprValueKind SubjectValueKind = Subject->getValueKind();
  MatchProjection *SharedHolding =
      findAlternativeHoldingProjection(ProjectionCache, Subject);
  MatchProjection *SharedDiscriminator = findAlternativeDiscriminatorProjection(
      S, ProjectionCache, Subject, Traits.Type);
  VarDecl *HoldingVar;
  if (SharedHolding && SharedHolding != Projection) {
    HoldingVar = SharedHolding->getHoldingVar();
    Projection->setHoldingVar(HoldingVar);
  } else {
    HoldingVar =
        BuildVarDecl(S, Loc, S.Context.getAutoRRefDeductType(), Subject);
    if (HoldingVar->isInvalidDecl())
      return true;
    Projection->setHoldingVar(HoldingVar);
  }
  if (SharedDiscriminator && SharedDiscriminator != Projection)
    Projection->setIntermediateVar(SharedDiscriminator->getIntermediateVar());
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

  if (!Pattern->getSubPattern())
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
  if (ProjectedValueKind == VK_PRValue) {
    Projection->setProjectedExpr(GetCall.get());
    return S.CheckCompleteMatchPattern(GetCall.get(), Pattern->getSubPattern(),
                                       State, ProjectionCache);
  }
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
      if (!P->getSubPattern())
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
        Expr *Element = getDecompositionElement(*this, Subject, Binding);
        if (CheckCompleteMatchPattern(Element, Child, State, ProjectionCache))
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
      Expr *Element = getDecompositionElement(*this, Subject, BD);
      if (CheckCompleteMatchPattern(Element, C, State, ProjectionCache)) {
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
  if (!State.CheckedBindingReferences) {
    State.CheckedBindingReferences = true;
    if (checkPatternBindingReferences(*this, Pattern))
      return true;
  }
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
      return MatchPatternRefutability::Refutable;

    case MatchPattern::DeclarationPatternClass:
      if (Info && Info->Projection &&
          Info->Projection->getKind() == MatchProjection::CastProjection)
        return MatchPatternRefutability::Refutable;
      return MatchPatternRefutability::Irrefutable;

    case MatchPattern::TypePatternClass:
      if (!Info || !Info->TypePatternResolved)
        return MatchPatternRefutability::Refutable;
      if (!Info->TypePatternMatches)
        return MatchPatternRefutability::Impossible;
      if (Info->Projection &&
          Info->Projection->getKind() == MatchProjection::CastProjection)
        return MatchPatternRefutability::Refutable;
      return MatchPatternRefutability::Irrefutable;

    case MatchPattern::AlternativePatternClass: {
      auto *Alternative = static_cast<AlternativePattern *>(P);
      if (!Info)
        return MatchPatternRefutability::Refutable;

      if (Info->IsOpenAlternative) {
        if (Info->OpenAlternativeProjectableWildcard &&
            !Info->OpenAlternativeHasEmpty)
          return MatchPatternRefutability::Irrefutable;
        return MatchPatternRefutability::Refutable;
      }

      if (Info->Projection && !Info->AlternativeProviderType.isNull()) {
        const VarDecl *HoldingVar = Info->Projection->getHoldingVar();
        assert(HoldingVar && HoldingVar->getInit() &&
               "closed alternative projection has no subject");
        MatchSemanticDomainConstraint Constraint;
        Constraint.Subject = HoldingVar->getInit();
        Constraint.ProviderType = Info->AlternativeProviderType;
        Constraint.Alternatives.append(Info->SelectedAlternatives.begin(),
                                       Info->SelectedAlternatives.end());
        Result.Domain.push_back(std::move(Constraint));
      }

      if (!Alternative->getSubPattern())
        return MatchPatternRefutability::Irrefutable;
      return Recurse(Alternative->getSubPattern(), Recurse);
    }

    case MatchPattern::DecompositionPatternClass: {
      MatchPatternRefutability Decomposition =
          MatchPatternRefutability::Irrefutable;
      for (MatchPattern *Child : P->children()) {
        MatchPatternRefutability ChildResult = Recurse(Child, Recurse);
        if (ChildResult == MatchPatternRefutability::Impossible)
          return MatchPatternRefutability::Impossible;
        if (ChildResult == MatchPatternRefutability::Refutable)
          Decomposition = MatchPatternRefutability::Refutable;
      }
      return Decomposition;
    }
    }
    llvm_unreachable("unknown match pattern kind");
  };

  Result.Refutability = Analyze(Pattern, Analyze);
  return Result;
}
