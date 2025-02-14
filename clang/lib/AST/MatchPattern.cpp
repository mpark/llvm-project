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
#include <algorithm>

using namespace clang;

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
  case BindingPatternClass:
    return "BindingPattern";
  case ParenPatternClass:
    return "ParenPattern";
  case OptionalPatternClass:
    return "OptionalPattern";
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
  case BindingPatternClass:
    return static_cast<const BindingPattern *>(this)->getBeginLoc();
  case ParenPatternClass:
    return static_cast<const ParenPattern *>(this)->getBeginLoc();
  case OptionalPatternClass:
    return static_cast<const OptionalPattern *>(this)->getBeginLoc();
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
  case BindingPatternClass:
    return static_cast<const BindingPattern *>(this)->getEndLoc();
  case ParenPatternClass:
    return static_cast<const ParenPattern *>(this)->getEndLoc();
  case OptionalPatternClass:
    return static_cast<const OptionalPattern *>(this)->getEndLoc();
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
  case BindingPatternClass:
    return static_cast<BindingPattern *>(this)->children();
  case ParenPatternClass:
    return static_cast<ParenPattern *>(this)->children();
  case OptionalPatternClass:
    return static_cast<OptionalPattern *>(this)->children();
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

SourceLocation BindingPattern::getEndLoc() const {
  return Binding->getEndLoc();
}

DecompositionPattern::DecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                           SourceRange Squares,
                                           bool BindingOnly)
    : MatchPattern(DecompositionPatternClass), NumPatterns(Patterns.size()),
      Squares(Squares), BindingOnly(BindingOnly) {
  std::uninitialized_copy(Patterns.begin(), Patterns.end(), getPatterns());
  setDependence(computeDependence());
}

DecompositionPattern *
DecompositionPattern::Create(const ASTContext &Ctx,
                             ArrayRef<MatchPattern *> Patterns,
                             SourceRange Squares, bool BindingOnly) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<MatchPattern *>(Patterns.size()));
  return new (Mem) DecompositionPattern(Patterns, Squares, BindingOnly);
}

DecompositionPattern *DecompositionPattern::CreateEmpty(const ASTContext &Ctx,
                                                        unsigned NumPatterns) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<MatchPattern *>(NumPatterns));
  return new (Mem) DecompositionPattern(NumPatterns);
}
