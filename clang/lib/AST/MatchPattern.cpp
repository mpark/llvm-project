//===- MatchPattern.cpp - MatchPattern AST Node Implementation ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the MatchPattern class and pattern subclasses.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/MatchPattern.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>

using namespace clang;

void *MatchProjection::operator new(size_t Bytes, const ASTContext &C,
                                    unsigned Alignment) {
  return ::operator new(Bytes, C, Alignment);
}

MatchPatternInstantiation *
MatchPatternInstantiation::Create(const ASTContext &Ctx, MatchPattern *Pattern,
                                  ArrayRef<MatchPatternInfo> Infos) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<MatchPatternInfo>(Infos.size()),
                           alignof(MatchPatternInstantiation));
  auto *Result = new (Mem) MatchPatternInstantiation(Pattern, Infos.size());
  std::uninitialized_copy(Infos.begin(), Infos.end(),
                          Result->getTrailingObjects());
  return Result;
}

const MatchPatternInfo *
MatchPatternInstantiation::find(const MatchPattern *P) const {
  for (const MatchPatternInfo &Info : infos())
    if (Info.Pattern == P)
      return &Info;
  return nullptr;
}

ArrayRef<MatchPattern *> MatchPatternInstantiation::getDecompositionPatterns(
    const DecompositionPattern *P) const {
  if (const MatchPatternInfo *Info = find(P); Info && Info->HasExpandedPatterns)
    return Info->ExpandedPatterns;
  return P->patterns();
}

void clang::visitMatchPatternEvaluation(
    const MatchPattern *Pattern, const MatchPatternInstantiation *Instantiation,
    llvm::function_ref<void(const Decl *)> VisitDecl,
    llvm::function_ref<void(const Stmt *)> VisitStmt) {
  llvm::SmallPtrSet<const Decl *, 8> VisitedDecls;
  llvm::SmallPtrSet<const Stmt *, 16> VisitedStmts;

  auto VisitStatement = [&](const Stmt *S) {
    if (S && VisitedStmts.insert(S).second)
      VisitStmt(S);
  };
  auto VisitDeclaration = [&](const Decl *D) {
    if (!D || !VisitedDecls.insert(D).second)
      return;
    VisitDecl(D);
    if (const auto *VD = dyn_cast<VarDecl>(D))
      VisitStatement(VD->getInit());
  };

  auto VisitPattern = [&](const MatchPattern *P, auto &Recurse) -> void {
    if (const auto *Expression = dyn_cast<ExpressionPattern>(P))
      VisitStatement(Expression->getExpr());
    else if (const auto *Declaration = dyn_cast<DeclarationPattern>(P))
      VisitDeclaration(Declaration->getDeclaration());
    if (const auto *Alternative = dyn_cast<AlternativePattern>(P))
      if (const MatchPattern *Selector = Alternative->getSelector())
        Recurse(Selector, Recurse);
    if (const auto *Decomposition = dyn_cast<DecompositionPattern>(P);
        Instantiation && Decomposition) {
      for (const MatchPattern *Child :
           Instantiation->getDecompositionPatterns(Decomposition))
        Recurse(Child, Recurse);
    } else {
      for (const MatchPattern *Child : P->children())
        Recurse(Child, Recurse);
    }
  };
  VisitPattern(Pattern, VisitPattern);

  if (!Instantiation)
    return;
  for (const MatchPatternInfo &Info : Instantiation->infos()) {
    VisitDeclaration(Info.TypePatternDeclaration);
    VisitStatement(Info.Condition);
    const MatchProjection *Projection = Info.Projection;
    if (!Projection)
      continue;
    VisitDeclaration(Projection->getHoldingVar());
    VisitDeclaration(Projection->getIntermediateVar());
    VisitDeclaration(Projection->getConditionVar());
    VisitDeclaration(Projection->getProjectedVar());
    VisitDeclaration(Projection->getDecomposedDecl());
    VisitStatement(Projection->getConditionExpr());
    VisitStatement(Projection->getProjectedExpr());
  }
}

void *MatchPattern::operator new(size_t bytes, const ASTContext &C,
                                 unsigned alignment) {
  return ::operator new(bytes, C, alignment);
}

const char *MatchPattern::getMatchPatternClassName() const {
  switch (Class) {
  case WildcardPatternClass:
    return "WildcardPattern";
  case ExpressionPatternClass:
    return "ExpressionPattern";
  case DeclarationPatternClass:
    return "DeclarationPattern";
  case TypePatternClass:
    return "TypePattern";
  case AlternativePatternClass:
    return "AlternativePattern";
  case DecompositionPatternClass:
    return "DecompositionPattern";
  }
  llvm_unreachable("unknown match pattern kind");
}

SourceLocation MatchPattern::getBeginLoc() const {
  switch (Class) {
  case WildcardPatternClass:
    return static_cast<const WildcardPattern *>(this)->getBeginLoc();
  case ExpressionPatternClass:
    return static_cast<const ExpressionPattern *>(this)->getBeginLoc();
  case DeclarationPatternClass:
    return static_cast<const DeclarationPattern *>(this)->getBeginLoc();
  case TypePatternClass:
    return static_cast<const TypePattern *>(this)->getBeginLoc();
  case AlternativePatternClass:
    return static_cast<const AlternativePattern *>(this)->getBeginLoc();
  case DecompositionPatternClass:
    return static_cast<const DecompositionPattern *>(this)->getBeginLoc();
  }
  llvm_unreachable("unknown match pattern kind");
}

SourceLocation MatchPattern::getEndLoc() const {
  switch (Class) {
  case WildcardPatternClass:
    return static_cast<const WildcardPattern *>(this)->getEndLoc();
  case ExpressionPatternClass:
    return static_cast<const ExpressionPattern *>(this)->getEndLoc();
  case DeclarationPatternClass:
    return static_cast<const DeclarationPattern *>(this)->getEndLoc();
  case TypePatternClass:
    return static_cast<const TypePattern *>(this)->getEndLoc();
  case AlternativePatternClass:
    return static_cast<const AlternativePattern *>(this)->getEndLoc();
  case DecompositionPatternClass:
    return static_cast<const DecompositionPattern *>(this)->getEndLoc();
  }
  llvm_unreachable("unknown match pattern kind");
}

llvm::iterator_range<MatchPattern **> MatchPattern::children() {
  switch (Class) {
  case WildcardPatternClass:
    return static_cast<WildcardPattern *>(this)->children();
  case ExpressionPatternClass:
    return static_cast<ExpressionPattern *>(this)->children();
  case DeclarationPatternClass:
    return static_cast<DeclarationPattern *>(this)->children();
  case TypePatternClass:
    return static_cast<TypePattern *>(this)->children();
  case AlternativePatternClass:
    return static_cast<AlternativePattern *>(this)->children();
  case DecompositionPatternClass:
    return static_cast<DecompositionPattern *>(this)->children();
  }
  llvm_unreachable("unknown match pattern kind");
}

ExpressionPattern::ExpressionPattern(Expr *E, bool IsPackExpansion)
    : MatchPattern(ExpressionPatternClass), E(E),
      IsPackExpansion(IsPackExpansion) {
  setDependence(E->getDependence());
}

SourceLocation ExpressionPattern::getBeginLoc() const {
  return E->getBeginLoc();
}

SourceLocation ExpressionPattern::getEndLoc() const {
  return E->getEndLoc();
}

DeclarationPattern::DeclarationPattern(VarDecl *Declaration,
                                       SourceRange WrittenRange,
                                       VarDecl *PackSourceDeclaration)
    : MatchPattern(DeclarationPatternClass), Declaration(Declaration),
      PackSourceDeclaration(PackSourceDeclaration ? PackSourceDeclaration
                                                  : Declaration),
      DeclarationRange(WrittenRange) {
  setDependence(toExprDependenceForImpliedType(
      Declaration->getType()->getDependence()));
}

AlternativePattern::AlternativePattern(SourceRange Braces,
                                       SourceRange ConstraintRange,
                                       ConceptReference *Constraint,
                                       SourceLocation ColonLoc,
                                       MatchPattern *Pattern)
    : MatchPattern(AlternativePatternClass), Kind(TypeConstraint),
      DiscriminatorRange(ConstraintRange), Braces(Braces),
      Constraint(Constraint), ColonLoc(ColonLoc), Pattern(Pattern) {
  ExprDependence Dependence = computeDependence();
  if (const ASTTemplateArgumentListInfo *Args =
          Constraint->getTemplateArgsAsWritten())
    for (const TemplateArgumentLoc &Arg : Args->arguments())
      Dependence |= toExprDependence(Arg.getArgument().getDependence());
  setDependence(Dependence);
}

SourceLocation DeclarationPattern::getBeginLoc() const {
  return DeclarationRange.getBegin();
}

SourceLocation DeclarationPattern::getEndLoc() const {
  return DeclarationRange.getEnd();
}

DecompositionPattern::DecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                           SourceRange Squares)
    : MatchPattern(DecompositionPatternClass), NumPatterns(Patterns.size()),
      Squares(Squares) {
  std::uninitialized_copy(Patterns.begin(), Patterns.end(), getPatterns());
  setDependence(computeDependence());
}

DecompositionPattern *
DecompositionPattern::Create(const ASTContext &Ctx,
                             ArrayRef<MatchPattern *> Patterns,
                             SourceRange Squares) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<MatchPattern *>(Patterns.size()));
  return new (Mem) DecompositionPattern(Patterns, Squares);
}

DecompositionPattern *DecompositionPattern::CreateEmpty(const ASTContext &Ctx,
                                                        unsigned NumPatterns) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<MatchPattern *>(NumPatterns));
  return new (Mem) DecompositionPattern(NumPatterns);
}
