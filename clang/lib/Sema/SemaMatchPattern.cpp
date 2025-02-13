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

#include "clang/AST/ExprCXX.h"
#include "clang/AST/MatchPattern.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/EnterExpressionEvaluationContext.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/SemaObjC.h"
#include "clang/Sema/TemplateDeduction.h"

using namespace clang;
using namespace sema;

static VarDecl *BuildVarDecl(Sema &SemaRef, SourceLocation Loc, QualType Type,
                             Expr *Init) {
  DeclContext *DC = SemaRef.CurContext;
  TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
  VarDecl *Decl = VarDecl::Create(SemaRef.Context, DC, Loc, {}, /*Id=*/nullptr,
                                  Type, TInfo, SC_None);
  Decl->setImplicit();
  // TODO: Consider ActOnInitializerError
  SemaRef.AddInitializerToDecl(Decl, Init, /*DirectInit=*/false);
  SemaRef.FinalizeDeclaration(Decl);
  return Decl;
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
                                     unsigned DiagID) {
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
  QualType TraitTy = S.CheckTemplateIdType(TemplateName(TraitTD), Loc, Args);
  if (TraitTy.isNull())
    return true;
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

// Copied and modified IsTupleLike from SemaDeclCXX.cpp
namespace {
enum class IsVariantLike { VariantLike, NotVariantLike, Error };
}

// Copied and modified isTupleLike from SemaDeclCXX.cpp
static IsVariantLike isVariantLike(Sema &S, SourceLocation Loc, QualType T,
                                   llvm::APSInt &Size) {
  EnterExpressionEvaluationContext ContextRAII(
      S, Sema::ExpressionEvaluationContext::ConstantEvaluated);

  DeclarationName Value = S.PP.getIdentifierInfo("value");
  LookupResult R(S, Value, Loc, Sema::LookupOrdinaryName);

  // Form template argument list for variant_size<T>.
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(getTrivialTypeTemplateArgument(S, Loc, T));

  // If there's no variant_size specialization or the lookup of 'value' is
  // empty, it's not variant-like.
  if (lookupStdTypeTraitMember(S, R, Loc, "variant_size", Args, /*DiagID*/ 0) ||
      R.empty())
    return IsVariantLike::NotVariantLike;

  // If we get this far, we've committed to the variant interpretation, but
  // we can still fail if there actually isn't a usable ::value.

  struct ICEDiagnoser : Sema::VerifyICEDiagnoser {
    LookupResult &R;
    TemplateArgumentListInfo &Args;
    ICEDiagnoser(LookupResult &R, TemplateArgumentListInfo &Args)
        : R(R), Args(Args) {}
    Sema::SemaDiagnosticBuilder diagnoseNotICE(Sema &S,
                                               SourceLocation Loc) override {
      return S.Diag(Loc, diag::err_alternative_pattern_std_variant_size_not_constant)
             << printTemplateArgs(S.Context.getPrintingPolicy(), Args,
                                  /*Params*/ nullptr);
    }
  } Diagnoser(R, Args);

  ExprResult E =
      S.BuildDeclarationNameExpr(CXXScopeSpec(), R, /*NeedsADL*/ false);
  if (E.isInvalid())
    return IsVariantLike::Error;

  E = S.VerifyIntegerConstantExpression(E.get(), &Size, Diagnoser);
  if (E.isInvalid())
    return IsVariantLike::Error;

  return IsVariantLike::VariantLike;
}

// Copied and modified getTupleLikeElementType from SemaDeclCXX.cpp
/// \return std::variant_alternative<I, T>::type.
static QualType getVariantLikeAlternativeType(Sema &S, SourceLocation Loc,
                                              unsigned I, QualType T) {
  // Form template argument list for variant_alternative<I, T>.
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(
      getTrivialIntegralTemplateArgument(S, Loc, S.Context.getSizeType(), I));
  Args.addArgument(getTrivialTypeTemplateArgument(S, Loc, T));

  DeclarationName TypeDN = S.PP.getIdentifierInfo("type");
  LookupResult R(S, TypeDN, Loc, Sema::LookupOrdinaryName);
  if (lookupStdTypeTraitMember(
          S, R, Loc, "variant_alternative", Args,
          diag::err_alternative_pattern_std_variant_alternative_not_specialized))
    return QualType();

  auto *TD = R.getAsSingle<TypeDecl>();
  if (!TD) {
    R.suppressDiagnostics();
    S.Diag(Loc, diag::err_alternative_pattern_std_variant_alternative_not_specialized)
        << printTemplateArgs(S.Context.getPrintingPolicy(), Args,
                             /*Params*/ nullptr);
    if (!R.empty())
      S.Diag(R.getRepresentativeDecl()->getLocation(), diag::note_declared_at);
    return QualType();
  }

  return S.Context.getTypeDeclType(TD);
}

static bool checkVariantLikeAlternative(Sema &S, VarDecl *HoldingVar,
                                        AlternativePattern *P, QualType Type,
                                        const llvm::APSInt &VariantSize) {
  SourceLocation Loc = P->getBeginLoc();

  DeclRefExpr *DRE = S.BuildDeclRefExpr(HoldingVar, Type, VK_LValue,
                                        HoldingVar->getLocation());

  DeclarationNameInfo IndexNameInfo(S.PP.getIdentifierInfo("index"), Loc);
  LookupResult MemberIndex(S, IndexNameInfo, Sema::LookupMemberName);
  bool UseMemberIndex = false;
  if (S.isCompleteType(HoldingVar->getLocation(), Type)) {
    if (auto *RD = Type->getAsCXXRecordDecl())
      S.LookupQualifiedName(MemberIndex, RD);
    if (MemberIndex.isAmbiguous())
      return true;
    for (NamedDecl *D : MemberIndex) {
      if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D->getUnderlyingDecl())) {
        if (FD->param_empty()) {
          UseMemberIndex = true;
          break;
        }
      }
    }
  }

  ExprResult IndexExpr;
  if (UseMemberIndex) {
    IndexExpr = S.BuildMemberReferenceExpr(
        DRE, Type, Loc, false, CXXScopeSpec(), SourceLocation(),
        nullptr, MemberIndex, nullptr, nullptr);
    if (IndexExpr.isInvalid())
      return true;

    IndexExpr = S.BuildCallExpr(nullptr, IndexExpr.get(), Loc, {}, Loc);
  } else {
    //   Otherwise, the initializer is index(e), where index is looked up
    //   in the associated namespaces.
    Expr *Index = UnresolvedLookupExpr::Create(
        S.Context, nullptr, NestedNameSpecifierLoc(), SourceLocation(),
        IndexNameInfo, /*RequiresADL=*/true, nullptr, UnresolvedSetIterator(),
        UnresolvedSetIterator(),
        /*KnownDependent=*/false, /*KnownInstantiationDependent=*/false);

    Expr *Arg = DRE;
    IndexExpr = S.BuildCallExpr(nullptr, Index, Loc, Arg, Loc);
  }
  if (IndexExpr.isInvalid())
    return true;

  unsigned NumAlternatives = VariantSize.getLimitedValue(UINT_MAX);
  // TODO(mpark): Cache this.
  llvm::SmallVector<QualType, 8> Alternatives;
  Alternatives.reserve(NumAlternatives);
  for (unsigned I = 0; I < NumAlternatives; ++I) {
    QualType Alternative =
        getVariantLikeAlternativeType(S, Loc, I, Type.getUnqualifiedType());
    if (Alternative.isNull())
      return true;
    Alternatives.push_back(Alternative);
  }
  unsigned I = 0;
  if (ConceptReference* CR = P->getConceptReference()) {
    CXXScopeSpec SS;
    SS.Adopt(CR->getNestedNameSpecifierLoc());

    for (; I < NumAlternatives; ++I) {
      TemplateArgumentListInfo TemplateArgs;
      TemplateArgs.addArgument(S.getTrivialTemplateArgumentLoc(
          TemplateArgument(Alternatives[I]), /*NTTPType=*/QualType(), Loc));
      if (const ASTTemplateArgumentListInfo *ArgsAsWritten =
              CR->getTemplateArgsAsWritten()) {
        TemplateArgs.setLAngleLoc(ArgsAsWritten->getLAngleLoc());
        TemplateArgs.setRAngleLoc(ArgsAsWritten->getRAngleLoc());
        for (const TemplateArgumentLoc &Arg : ArgsAsWritten->arguments()) {
          TemplateArgs.addArgument(Arg);
        }
      }
      ExprResult E = S.CheckConceptTemplateId(
          SS, CR->getTemplateKWLoc(), CR->getConceptNameInfo(),
          CR->getFoundDecl(), CR->getNamedConcept(), &TemplateArgs);
      if (E.isInvalid())
        return true;
      ConceptSpecializationExpr* CSE = cast<ConceptSpecializationExpr>(E.get());
      if (CSE->isSatisfied()) {
        break;
      }
    }
    if (I == NumAlternatives) {
      // S.Diag(Loc, diag::err_no_viable_alternative)
      //     << *CR << Type.getUnqualifiedType().getAsString();
      return true;
    }
  } else if (TypeSourceInfo* TSI = P->getTypeSourceInfo()) {
    QualType TargetType = TSI->getType();
    for (; I < NumAlternatives; ++I) {
      if (S.Context.hasSameType(Alternatives[I], TargetType)) {
        break;
      }
    }
    if (I == NumAlternatives) {
      S.Diag(Loc, diag::err_no_viable_alternative)
          << TargetType << Type.getUnqualifiedType().getAsString();
      return true;
    }
  }

  ExprResult TargetIndex = S.ActOnIntegerConstant(Loc, I);
  if (TargetIndex.isInvalid())
    return true;

  ExprResult Cond =
      S.ActOnBinOp(S.getCurScope(), Loc, tok::TokenKind::equalequal,
                   IndexExpr.get(), TargetIndex.get());
  if (Cond.isInvalid()) {
    return true;
  }
  P->setCond(Cond.get());

  DeclarationName GetDN = S.PP.getIdentifierInfo("get");

  LookupResult MemberGet(S, GetDN, Loc, Sema::LookupMemberName);
  bool UseMemberGet = false;
  if (S.isCompleteType(HoldingVar->getLocation(), Type)) {
    if (auto *RD = Type->getAsCXXRecordDecl())
      S.LookupQualifiedName(MemberGet, RD);
    if (MemberGet.isAmbiguous())
      return true;
    //   ... and if that finds at least one declaration that is a function
    //   template whose first template parameter is a non-type parameter ...
    for (NamedDecl *D : MemberGet) {
      if (FunctionTemplateDecl *FTD =
              dyn_cast<FunctionTemplateDecl>(D->getUnderlyingDecl())) {
        TemplateParameterList *TPL = FTD->getTemplateParameters();
        if (TPL->size() != 0 &&
            isa<NonTypeTemplateParmDecl>(TPL->getParam(0))) {
          //   ... the initializer is e.get<i>().
          UseMemberGet = true;
          break;
        }
      }
    }
  }

  ExprResult E = DRE;
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(
      getTrivialIntegralTemplateArgument(S, Loc, S.Context.getSizeType(), I));

  if (UseMemberGet) {
    //   if [lookup of member get] finds at least one declaration, the
    //   initializer is e.get<i>().
    E = S.BuildMemberReferenceExpr(E.get(), Type, Loc, false, CXXScopeSpec(),
                                   SourceLocation(), nullptr, MemberGet, &Args,
                                   nullptr);
    if (E.isInvalid())
      return true;

    E = S.BuildCallExpr(nullptr, E.get(), Loc, std::nullopt, Loc);
  } else {
    //   Otherwise, the initializer is get<i>(e), where get is looked up
    //   in the associated namespaces.
    Expr *Get = UnresolvedLookupExpr::Create(
        S.Context, nullptr, NestedNameSpecifierLoc(), SourceLocation(),
        DeclarationNameInfo(GetDN, Loc), /*RequiresADL=*/true, &Args,
        UnresolvedSetIterator(), UnresolvedSetIterator(),
        /*KnownDependent=*/false, /*KnownInstantiationDependent=*/false);

    Expr *Arg = E.get();
    E = S.BuildCallExpr(nullptr, Get, Loc, Arg, Loc);
  }
  if (E.isInvalid())
    return true;

  Expr *Init = E.get();

  //   Given the type T designated by std::variant_alternative<i, E>::type,
  QualType T = getVariantLikeAlternativeType(S, Loc, I, Type);
  if (T.isNull())
    return true;

  //   each vi is a variable of type "reference to T" initialized with the
  //   initializer, where the reference is an lvalue reference if the
  //   initializer is an lvalue and an rvalue reference otherwise
  QualType RefType = S.BuildReferenceType(T, E.get()->isLValue(), Loc, {});
  if (RefType.isNull())
    return true;
  auto *RefVD = VarDecl::Create(S.Context, HoldingVar->getDeclContext(), Loc,
                                Loc, nullptr, RefType,
                                S.Context.getTrivialTypeSourceInfo(T, Loc),
                                HoldingVar->getStorageClass());
  RefVD->setLexicalDeclContext(HoldingVar->getLexicalDeclContext());
  RefVD->setTSCSpec(HoldingVar->getTSCSpec());
  RefVD->setImplicit();
  if (HoldingVar->isInlineSpecified())
    RefVD->setInlineSpecified();
  RefVD->getLexicalDeclContext()->addHiddenDecl(RefVD);

  InitializedEntity Entity = InitializedEntity::InitializeBinding(RefVD);
  InitializationKind Kind = InitializationKind::CreateCopy(Loc, Loc);
  InitializationSequence Seq(S, Entity, Kind, Init);
  E = Seq.Perform(S, Entity, Kind, Init);
  if (E.isInvalid())
    return true;
  E = S.ActOnFinishFullExpr(E.get(), Loc, /*DiscardedValue*/ false);
  if (E.isInvalid())
    return true;
  RefVD->setInit(E.get());
  S.CheckCompleteVariableDeclaration(RefVD);

  P->setBindingVar(RefVD);

  E = S.BuildDeclRefExpr(RefVD, RefVD->getType().getNonReferenceType(),
                         VK_LValue, RefVD->getLocation());
  if (E.isInvalid())
    return true;

  return S.CheckCompleteMatchPattern(E.get(), P->getSubPattern());
}

ExprResult Sema::ActOnMatchSubject(Expr *Subject, VarDecl *&HoldingVar) {
  QualType Deduced = Context.getAutoRRefDeductType();
  VarDecl *VD = BuildVarDecl(*this, Subject->getExprLoc(), Deduced, Subject);
  if (VD->isInvalidDecl()) {
    return ExprError();
  }
  HoldingVar = VD;
  return BuildDeclRefExpr(HoldingVar,
                          HoldingVar->getType().getNonReferenceType(),
                          VK_LValue, HoldingVar->getLocation());
}

StmtResult Sema::ActOnMatchExprHandler(TypeLoc OrigResultType, QualType &RetTy,
                                       ExprResult ER) {
  if (ER.isInvalid()) {
    return StmtError();
  }
  Expr *E = ER.get();
  SourceLocation Loc = E->getBeginLoc();
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
  if (ER.isInvalid()) {
    return StmtError();
  }
  E = ER.get();
  CheckReturnValExpr(E, RetTy, Loc);
  ER = ActOnFinishFullExpr(E, Loc, /*DiscardedValue=*/false);
  if (ER.isInvalid()) {
    return StmtError();
  }
  return ER.get();
}

ExprResult Sema::ActOnMatchTestExpr(VarDecl *HoldingVar, Expr *Subject,
                                    SourceLocation MatchLoc,
                                    MatchPattern *Pattern, SourceLocation IfLoc,
                                    MatchGuard Guard) {
  return new (Context) MatchTestExpr(Context, HoldingVar, Subject, MatchLoc,
                                     Pattern, IfLoc, Guard);
}

ExprResult Sema::ActOnMatchSelectExpr(Expr *Subject, SourceLocation MatchLoc,
                                      bool IsConstexpr, QualType RetTy,
                                      ArrayRef<MatchCase> Cases,
                                      SourceRange Braces) {
  return MatchSelectExpr::Create(Context, Subject, MatchLoc, IsConstexpr, RetTy,
                                 Cases, Braces);
}

ActionResult<MatchPattern *>
Sema::ActOnWildcardPattern(SourceLocation WildcardLoc) {
  return new (Context) WildcardPattern(WildcardLoc);
}

ActionResult<MatchPattern *> Sema::ActOnExpressionPattern(Expr *E) {
  return new (Context) ExpressionPattern(E);
}

ActionResult<MatchPattern *> Sema::ActOnBindingPattern(SourceLocation LetLoc,
                                                       SourceLocation NameLoc,
                                                       IdentifierInfo *Name) {
  BindingDecl *BD = BindingDecl::Create(Context, CurContext, NameLoc, Name);
  PushOnScopeChains(BD, getCurScope());
  return new (Context) BindingPattern(LetLoc, BD);
}

ActionResult<MatchPattern *> Sema::ActOnParenPattern(SourceRange Parens,
                                                     MatchPattern *SubPattern) {
  assert(SubPattern->getMatchPatternClass() !=
             MatchPattern::ExpressionPatternClass &&
         "Parenthesized pattern shouldn't contain an expression.");
  return new (Context) ParenPattern(Parens, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnOptionalPattern(SourceLocation QuestionLoc,
                           MatchPattern *SubPattern) {
  return new (Context) OptionalPattern(QuestionLoc, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnAlternativePattern(SourceRange DiscriminatorRange,
                              ConceptReference *CR, SourceLocation ColonLoc,
                              MatchPattern *SubPattern) {
  return new (Context)
      AlternativePattern(DiscriminatorRange, CR, ColonLoc, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnAlternativePattern(SourceRange DiscriminatorRange,
                              TypeSourceInfo *TSI, SourceLocation ColonLoc,
                              MatchPattern *SubPattern) {
  return new (Context)
      AlternativePattern(DiscriminatorRange, TSI, ColonLoc, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnDecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares, bool BindingOnly) {
  return DecompositionPattern::Create(Context, Patterns, Squares, BindingOnly);
}

bool Sema::CheckCompleteMatchPattern(Expr *Subject, MatchPattern *Pattern) {
  if (Subject && Subject->isTypeDependent())
    return CheckCompleteMatchPattern(nullptr, Pattern);
  // TODO(mpark): Skip if Pattern is type-dependent as well.
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
    P->setCond(Cond.get());
    break;
  }
  case MatchPattern::BindingPatternClass: {
    BindingPattern *P = static_cast<BindingPattern *>(Pattern);
    // If the type of the subject is dependent, then so is the binding.
    QualType Type = Subject ? Subject->getType() : Context.DependentTy;
    if (Subject && !Subject->refersToBitField()) {
      QualType Deduced = Context.getAutoRRefDeductType();
      VarDecl *HoldingVar = BuildVarDecl(*this, Loc, Deduced, Subject);
      if (HoldingVar->isInvalidDecl()) {
        return true;
      }
      Subject = BuildDeclRefExpr(HoldingVar,
                                 HoldingVar->getType().getNonReferenceType(),
                                 VK_LValue, HoldingVar->getLocation());
    }
    BindingDecl *BD = P->getBinding();
    BD->setBinding(Type, Subject);
    BD->setDecomposedDecl(nullptr);
    break;
  }
  case MatchPattern::ParenPatternClass: {
    ParenPattern *P = static_cast<ParenPattern *>(Pattern);
    return CheckCompleteMatchPattern(Subject, P->getSubPattern());
  }
  case MatchPattern::OptionalPatternClass: {
    OptionalPattern *P = static_cast<OptionalPattern *>(Pattern);
    if (!Subject)
      return CheckCompleteMatchPattern(nullptr, P->getSubPattern());
    QualType Type = Context.getAutoRRefDeductType();
    VarDecl *CondVar = BuildVarDecl(*this, Loc, Type, Subject);
    if (CondVar->isInvalidDecl()) {
      return true;
    }
    P->setCondVar(CondVar);
    DeclRefExpr *DRE =
        BuildDeclRefExpr(CondVar, CondVar->getType().getNonReferenceType(),
                         VK_LValue, CondVar->getLocation());
    ExprResult Cond = CheckBooleanCondition(Loc, DRE);
    if (Cond.isInvalid()) {
      return true;
    }
    P->setCond(Cond.get());
    ExprResult Deref = ActOnUnaryOp(S, Loc, tok::TokenKind::star, DRE);
    if (Deref.isInvalid()) {
      return true;
    }
    return CheckCompleteMatchPattern(Deref.get(), P->getSubPattern());
  }
  case MatchPattern::AlternativePatternClass: {
    AlternativePattern *P = static_cast<AlternativePattern *>(Pattern);
    if (!Subject)
      return CheckCompleteMatchPattern(nullptr, P->getSubPattern());
    QualType Deduced = Context.getAutoRRefDeductType();
    VarDecl *HoldingVar = BuildVarDecl(*this, Loc, Deduced, Subject);
    if (HoldingVar->isInvalidDecl())
      return true;
    P->setHoldingVar(HoldingVar);
    SourceLocation Loc = P->getBeginLoc();
    QualType Type = HoldingVar->getType();
    Type = Type.getNonReferenceType();

    llvm::APSInt VariantSize(32);
    switch (isVariantLike(*this, Loc, Type, VariantSize)) {
    case IsVariantLike::Error:
      return true;
    case IsVariantLike::VariantLike:
      return checkVariantLikeAlternative(*this, HoldingVar, P, Type, VariantSize);
    case IsVariantLike::NotVariantLike:
      break;
    }

    DeclRefExpr *DRE = BuildDeclRefExpr(HoldingVar, Type, VK_LValue,
                                        HoldingVar->getLocation());

    DeclarationNameInfo TryCastNameInfo(PP.getIdentifierInfo("try_cast"),
                                        P->getBeginLoc());
    OverloadCandidateSet CandidateSet(Loc, OverloadCandidateSet::CSK_Normal);

    QualType TargetType = P->getTypeSourceInfo()->getType();
    QualType VarType = Context.getAutoRRefDeductType();
    ExprResult CastExpr =
        BuildTryCastCall(Loc, TryCastNameInfo, &CandidateSet, TargetType, DRE);

    if (CastExpr.isUnset()) {
        ExprResult AddrOf = ActOnUnaryOp(S, Loc, tok::TokenKind::amp, Subject);
        if (AddrOf.isInvalid())
          return true;
        VarType = Context.getPointerType(
            Type.isConstQualified() ? TargetType.withConst() : TargetType);
        TypeSourceInfo *TSI = Context.getTrivialTypeSourceInfo(VarType, Loc);
        CastExpr = BuildCXXNamedCast({}, tok::kw_dynamic_cast, TSI,
                                            AddrOf.get(), {}, {});
    }

    if (CastExpr.isInvalid())
      return true;

    VarDecl *CondVar = BuildVarDecl(*this, Loc, VarType, CastExpr.get());
    if (CondVar->isInvalidDecl())
      return true;
    P->setCondVar(CondVar);
    DRE = BuildDeclRefExpr(CondVar, CondVar->getType().getNonReferenceType(),
                           VK_LValue, CondVar->getLocation());
    ExprResult Cond = CheckBooleanCondition(Loc, DRE);
    if (Cond.isInvalid()) {
      return true;
    }
    P->setCond(Cond.get());
    ExprResult Deref = ActOnUnaryOp(S, Loc, tok::TokenKind::star, DRE);
    if (Deref.isInvalid()) {
      return true;
    }
    return CheckCompleteMatchPattern(Deref.get(), P->getSubPattern());
  }
  case MatchPattern::DecompositionPatternClass: {
    DecompositionPattern *P = static_cast<DecompositionPattern *>(Pattern);
    if (!Subject) {
      for (MatchPattern *C : P->children()) {
        if (CheckCompleteMatchPattern(nullptr, C))
          return true;
      }
      return false;
    }
    QualType Type = Context.getAutoRRefDeductType();
    TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(Type, Loc);
    SmallVector<BindingDecl *, 8> Bindings;
    Bindings.reserve(P->getNumPatterns());
    for (MatchPattern *C : P->children()) {
      BindingDecl *BD = nullptr;
      if (C->getMatchPatternClass() == MatchPattern::BindingPatternClass) {
        BD = static_cast<BindingPattern *>(C)->getBinding();
      } else {
        BD =
            BindingDecl::Create(Context, CurContext, C->getBeginLoc(), nullptr);
        BD->setImplicit();
      }
      Bindings.push_back(BD);
    }
    DecompositionDecl *Decomposed = DecompositionDecl::Create(
        Context, CurContext, Loc, Loc, Type, TInfo, SC_None, Bindings);
    P->setDecomposedDecl(Decomposed);
    Decomposed->setImplicit();
    // TODO: Consider ActOnInitializerError
    AddInitializerToDecl(Decomposed, Subject, /*DirectInit=*/false);
    if (Decomposed->isInvalidDecl()) {
      return true;
    }
    unsigned I = 0;
    for (MatchPattern *C : P->children()) {
      BindingDecl *BD = Bindings[I];
      if (C->getMatchPatternClass() != MatchPattern::BindingPatternClass &&
          CheckCompleteMatchPattern(BD->getBinding(), C)) {
        return true;
      }
      ++I;
    }
    break;
  }
  }
  return false;
}
