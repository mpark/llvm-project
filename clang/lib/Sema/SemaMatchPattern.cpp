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

ExprResult Sema::ActOnMatchTestExpr(Expr *Subject, SourceLocation MatchLoc,
                                    MatchPattern *Pattern) {
  return new (Context) MatchTestExpr(Context, Subject, MatchLoc, Pattern);
}

ExprResult Sema::ActOnMatchSelectExpr(Expr *Subject, SourceLocation MatchLoc,
                                      bool IsConstexpr,
                                      ParsedType TrailingReturnType,
                                      ArrayRef<MatchCase> Cases,
                                      SourceRange Braces) {
  TypeSourceInfo *TSI = nullptr;
  QualType RetTy;
  if (TrailingReturnType) {
    RetTy = GetTypeFromParser(TrailingReturnType, &TSI);
  } else {
    RetTy = Context.getAutoDeductType();
    TSI = Context.CreateTypeSourceInfo(RetTy);
  }

  TypeLoc OrigResultType = TSI->getTypeLoc();
  for (const MatchCase& Case : Cases) {
    if (const AutoType *AT = RetTy->getContainedAutoType()) {
      if (Expr *E = dyn_cast<Expr>(Case.Handler)) {
        DeduceAutoTypeFromExpr(OrigResultType, E->getBeginLoc(), E, RetTy, AT);
      }
    }
  }

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

ActionResult<MatchPattern *> Sema::ActOnBindingPattern(SourceLocation NameLoc,
                                                       IdentifierInfo *Name) {
  BindingDecl *Binding =
      BindingDecl::Create(Context, CurContext, NameLoc, Name);
  PushOnScopeChains(Binding, getCurScope());
  return new (Context) BindingPattern(Binding);
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
  case MatchPattern::OptionalPatternClass: {
    OptionalPattern *P = static_cast<OptionalPattern *>(Pattern);
    ExprResult Cond =
        CheckBooleanCondition(P->getBeginLoc(), Subject);
    if (Cond.isInvalid()) {
      return true;
    }
    P->setCond(Cond.get());
    ExprResult Deref = ActOnUnaryOp(getCurScope(), P->getBeginLoc(), tok::TokenKind::star, Subject);
    if (Deref.isInvalid()) {
      return true;
    }
    return CheckCompleteMatchPattern(Deref.get(), P->getSubPattern());
  }
  case MatchPattern::DecompositionPatternClass:
    return false;
    DecompositionPattern *P = static_cast<DecompositionPattern *>(Pattern);
    SmallVector<BindingDecl*, 8> Bindings(P->getNumPatterns());
    unsigned I = 0;
    for (MatchPattern *C : P->children()) {
      switch (C->getMatchPatternClass()) {
      case MatchPattern::WildcardPatternClass:
        Bindings.push_back(nullptr);
        break;
      case MatchPattern::BindingPatternClass:
        Bindings.push_back(static_cast<BindingPattern *>(C)->getBinding());
        break;
      default: {
        std::string Name = "__decomp" + std::to_string(I);
        IdentifierInfo *II = &PP.getIdentifierTable().get(Name);
        Bindings.push_back(
            BindingDecl::Create(Context, CurContext, C->getBeginLoc(), II));
      }
      }
      ++I;
    }
    DecompositionDecl::Create(Context, CurContext, P->getBeginLoc(),
                              P->getSquares().getBegin(), QualType(), nullptr,
                              {}, Bindings);
    break;
  }
  return false;
}
