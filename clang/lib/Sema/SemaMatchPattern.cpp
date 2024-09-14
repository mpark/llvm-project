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
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/SemaObjC.h"
#include "clang/Sema/TemplateDeduction.h"

using namespace clang;
using namespace sema;

// NOTE: BuildForRangeVarDecl and FinishForRangeVarDecl are copied from
//       SemaStmt.cpp for now for prototyping purposes.

/// Build a variable declaration for a for-range statement.
static VarDecl *BuildForRangeVarDecl(Sema &SemaRef, SourceLocation Loc,
                                     QualType Type, StringRef Name) {
  DeclContext *DC = SemaRef.CurContext;
  IdentifierInfo *II = &SemaRef.PP.getIdentifierTable().get(Name);
  TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
  VarDecl *Decl = VarDecl::Create(SemaRef.Context, DC, Loc, Loc, II, Type,
                                  TInfo, SC_None);
  Decl->setImplicit();
  return Decl;
}

/// Finish building a variable declaration for a for-range statement.
/// \return true if an error occurs.
static bool FinishForRangeVarDecl(Sema &SemaRef, VarDecl *Decl, Expr *Init,
                                  SourceLocation Loc, int DiagID) {
  if (Decl->getType()->isUndeducedType()) {
    ExprResult Res = SemaRef.CorrectDelayedTyposInExpr(Init);
    if (!Res.isUsable()) {
      Decl->setInvalidDecl();
      return true;
    }
    Init = Res.get();
  }

  // Deduce the type for the iterator variable now rather than leaving it to
  // AddInitializerToDecl, so we can produce a more suitable diagnostic.
  QualType InitType;
  if (!isa<InitListExpr>(Init) && Init->getType()->isVoidType()) {
    SemaRef.Diag(Loc, DiagID) << Init->getType();
  } else {
    TemplateDeductionInfo Info(Init->getExprLoc());
    TemplateDeductionResult Result = SemaRef.DeduceAutoType(
        Decl->getTypeSourceInfo()->getTypeLoc(), Init, InitType, Info);
    if (Result != TemplateDeductionResult::Success &&
        Result != TemplateDeductionResult::AlreadyDiagnosed)
      SemaRef.Diag(Loc, DiagID) << Init->getType();
  }

  if (InitType.isNull()) {
    Decl->setInvalidDecl();
    return true;
  }
  Decl->setType(InitType);

  // In ARC, infer lifetime.
  // FIXME: ARC may want to turn this into 'const __unsafe_unretained' if
  // we're doing the equivalent of fast iteration.
  if (SemaRef.getLangOpts().ObjCAutoRefCount &&
      SemaRef.ObjC().inferObjCARCLifetime(Decl))
    Decl->setInvalidDecl();

  SemaRef.AddInitializerToDecl(Decl, Init, /*DirectInit=*/false);
  SemaRef.FinalizeDeclaration(Decl);
  SemaRef.CurContext->addDecl(Decl);
  return false;
}

ExprResult Sema::ActOnMatchSubject(Expr *Subject) {
  SourceLocation SubjectLoc = Subject->getBeginLoc();
  VarDecl *SubjectVar = BuildForRangeVarDecl(
      *this, SubjectLoc, Context.getAutoRRefDeductType(), "__match");
  if (FinishForRangeVarDecl(*this, SubjectVar, Subject, SubjectLoc,
                            diag::err_for_range_deduction_failure)) {
    return ExprError();
  }
  return BuildDeclRefExpr(SubjectVar,
                          SubjectVar->getType().getNonReferenceType(),
                          VK_LValue, SubjectVar->getLocation());
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
  Sema::NamedReturnInfo NRInfo = getNamedReturnInfo(E);
  auto Entity = InitializedEntity::InitializeStmtExprResult(Loc, RetTy);
  ER = PerformMoveOrCopyInitialization(Entity, NRInfo, E);
  if (ER.isInvalid()) {
    return StmtError();
  }
  E = ER.get();
  CheckReturnValExpr(E, RetTy, E->getBeginLoc());
  ER = ActOnFinishFullExpr(E, E->getBeginLoc(),
                           /*DiscardedValue=*/false);
  if (ER.isInvalid()) {
    return StmtError();
  }
  return ER.get();
}

ExprResult Sema::ActOnMatchTestExpr(Expr *Subject, SourceLocation MatchLoc,
                                    MatchPattern *Pattern) {
  return new (Context) MatchTestExpr(Context, Subject, MatchLoc, Pattern);
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
  BindingDecl *Binding =
      BindingDecl::Create(Context, CurContext, NameLoc, Name);
  PushOnScopeChains(Binding, getCurScope());
  return new (Context) BindingPattern(LetLoc, Binding);
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
Sema::ActOnDecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares, bool BindingOnly) {
  return DecompositionPattern::Create(Context, Patterns, Squares, BindingOnly);
}

bool Sema::CheckCompleteMatchPattern(Expr *Subject, MatchPattern *Pattern) {
  switch (Pattern->getMatchPatternClass()) {
  case MatchPattern::WildcardPatternClass:
    break;
  case MatchPattern::ExpressionPatternClass: {
    ExpressionPattern *P = static_cast<ExpressionPattern *>(Pattern);
    ExprResult Cond =
        ActOnBinOp(getCurScope(), P->getBeginLoc(), tok::TokenKind::equalequal,
                   Subject, P->getExpr());
    if (Cond.isInvalid()) {
      return true;
    }
    P->setCond(Cond.get());
    break;
  }
  case MatchPattern::BindingPatternClass: {
    BindingPattern *P = static_cast<BindingPattern *>(Pattern);
    BindingDecl *BD = P->getBinding();
    BD->setBinding(Subject->getType(), Subject);
    break;
  }
  case MatchPattern::ParenPatternClass: {
    ParenPattern *P = static_cast<ParenPattern *>(Pattern);
    return CheckCompleteMatchPattern(Subject, P->getSubPattern());
  }
  case MatchPattern::OptionalPatternClass: {
    OptionalPattern *P = static_cast<OptionalPattern *>(Pattern);
    ExprResult Cond = CheckBooleanCondition(P->getBeginLoc(), Subject);
    if (Cond.isInvalid()) {
      return true;
    }
    P->setCond(Cond.get());
    ExprResult Deref = ActOnUnaryOp(getCurScope(), P->getBeginLoc(),
                                    tok::TokenKind::star, Subject);
    if (Deref.isInvalid()) {
      return true;
    }
    return CheckCompleteMatchPattern(Deref.get(), P->getSubPattern());
  }
  case MatchPattern::DecompositionPatternClass:
    DecompositionPattern *P = static_cast<DecompositionPattern *>(Pattern);
    QualType Type = Context.getAutoRRefDeductType();
    SourceLocation Loc = P->getBeginLoc();
    TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
    SmallVector<BindingDecl*, 8> Bindings;
    Bindings.reserve(P->getNumPatterns());
    for (MatchPattern *C : P->children()) {
      BindingDecl *Binding =
          C->getMatchPatternClass() == MatchPattern::BindingPatternClass
              ? static_cast<BindingPattern *>(C)->getBinding()
              : BindingDecl::Create(Context, CurContext, C->getBeginLoc(),
                                    nullptr);
      Bindings.push_back(Binding);
    }
    DecompositionDecl *Decomposed = DecompositionDecl::Create(
        Context, CurContext, Loc, Loc, Type, TInfo, SC_None, Bindings);
    // TODO: Consider ActOnInitializerError
    AddInitializerToDecl(Decomposed, Subject, /*DirectInit=*/false);
    if (Decomposed->isInvalidDecl()) {
      return true;
    }
    P->setDecomposedDecl(Decomposed);
    unsigned I = 0;
    for (MatchPattern *C : P->children()) {
      BindingDecl *Binding = Bindings[I];
      switch (C->getMatchPatternClass()) {
      case MatchPattern::BindingPatternClass:
        break;
      default:
        if (CheckCompleteMatchPattern(Binding->getBinding(), C)) {
          return true;
        }
        break;
      }
      ++I;
    }
    break;
  }
  return false;
}
