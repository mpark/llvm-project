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

static VarDecl *BuildVarDecl(Sema &SemaRef, SourceLocation Loc, QualType Type,
                             TypeSourceInfo *TSI, Expr *Init) {
  DeclContext *DC = SemaRef.CurContext;
  VarDecl *Decl = VarDecl::Create(SemaRef.Context, DC, Loc, {}, /*Id=*/nullptr,
                                  Type, TSI, SC_None);
  Decl->setImplicit();
  // TODO: Consider ActOnInitializerError
  SemaRef.AddInitializerToDecl(Decl, Init, /*DirectInit=*/false);
  SemaRef.FinalizeDeclaration(Decl);
  return Decl;
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
  CheckReturnValExpr(E, RetTy, Loc);
  ER = ActOnFinishFullExpr(E, Loc, /*DiscardedValue=*/false);
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
Sema::ActOnAlternativePattern(SourceRange TypeRange, ParsedType Ty,
                              SourceLocation ColonLoc,
                              MatchPattern *SubPattern) {
  TypeSourceInfo *TSI = nullptr;
  GetTypeFromParser(Ty, &TSI);
  return new (Context) AlternativePattern(TypeRange, TSI, ColonLoc, SubPattern);
}

ActionResult<MatchPattern *>
Sema::ActOnDecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares, bool BindingOnly) {
  return DecompositionPattern::Create(Context, Patterns, Squares, BindingOnly);
}

bool Sema::CheckCompleteMatchPattern(Expr *Subject, MatchPattern *Pattern) {
  SourceLocation Loc = Pattern->getBeginLoc();
  Scope *S = getCurScope();
  switch (Pattern->getMatchPatternClass()) {
  case MatchPattern::WildcardPatternClass:
    break;
  case MatchPattern::ExpressionPatternClass: {
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
    QualType Type = Subject->getType();
    if (!Subject->refersToBitField()) {
      QualType Deduced = Context.getAutoRRefDeductType();
      TypeSourceInfo *TSI =
          SemaRef.Context.getTrivialTypeSourceInfo(Deduced, Loc);
      VarDecl *HoldingVar = BuildVarDecl(*this, Loc, Deduced, TSI, Subject);
      if (HoldingVar->isInvalidDecl()) {
        return true;
      }
      Subject = BuildDeclRefExpr(HoldingVar,
                                 HoldingVar->getType().getNonReferenceType(),
                                 VK_LValue, HoldingVar->getLocation());
    }
    BindingDecl *BD = P->getBinding();
    BD->setBinding(Type, Subject);
    break;
  }
  case MatchPattern::ParenPatternClass: {
    ParenPattern *P = static_cast<ParenPattern *>(Pattern);
    return CheckCompleteMatchPattern(Subject, P->getSubPattern());
  }
  case MatchPattern::OptionalPatternClass: {
    OptionalPattern *P = static_cast<OptionalPattern *>(Pattern);
    QualType Type = Context.getAutoRRefDeductType();
    TypeSourceInfo *TSI = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
    VarDecl *CondVar = BuildVarDecl(*this, Loc, Type, TSI, Subject);
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
    QualType Type = P->getTypeSourceInfo()->getType();
    Type = Context.getPointerType(
        Subject->getType().isConstQualified() ? Type.withConst() : Type);
    TypeSourceInfo *TSI = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
    ExprResult AddrOf = ActOnUnaryOp(S, Loc, tok::TokenKind::amp, Subject);
    if (AddrOf.isInvalid()) {
      return true;
    }
    ExprResult Cast =
        BuildCXXNamedCast({}, tok::kw_dynamic_cast, TSI, AddrOf.get(), {}, {});
    if (Cast.isInvalid()) {
      return true;
    }
    VarDecl *CondVar = BuildVarDecl(*this, Loc, Type, TSI, Cast.get());
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
  case MatchPattern::DecompositionPatternClass:
    DecompositionPattern *P = static_cast<DecompositionPattern *>(Pattern);
    QualType Type = Context.getAutoRRefDeductType();
    TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
    SmallVector<BindingDecl*, 8> Bindings;
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
  return false;
}
