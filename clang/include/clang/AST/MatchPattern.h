//===--- Pattern.h - Classes for representing C++ patterns ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the C++ pattern AST node classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PATTERN_H
#define LLVM_CLANG_AST_PATTERN_H

#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/ErrorHandling.h"

namespace clang {

class ASTContext;
class Expr;

class MatchPattern {
public:
  enum MatchPatternClass {
    WildcardPatternClass,
    ExpressionPatternClass,
    OptionalPatternClass,
  };

protected:
  // Make vanilla 'new' and 'delete' illegal for Stmts.
  void *operator new(size_t bytes) noexcept {
    llvm_unreachable("MatchPatterns cannot be allocated with regular 'new'.");
  }

  void operator delete(void *data) noexcept {
    llvm_unreachable("MatchPatterns cannot be released with regular 'delete'.");
  }

private:
  MatchPatternClass Class;
  SourceLocation PatternLoc;

public:
  // Only allow allocation of Stmts using the allocator in ASTContext
  // or by doing a placement new.
  void* operator new(size_t bytes, const ASTContext& C,
                     unsigned alignment = 8);

  void* operator new(size_t bytes, const ASTContext* C,
                     unsigned alignment = 8) {
    return operator new(bytes, *C, alignment);
  }

  void *operator new(size_t bytes, void *mem) noexcept { return mem; }

  void operator delete(void *, const ASTContext &, unsigned) noexcept {}
  void operator delete(void *, const ASTContext *, unsigned) noexcept {}
  void operator delete(void *, size_t) noexcept {}
  void operator delete(void *, void *) noexcept {}

protected:
  explicit MatchPattern(MatchPatternClass MPC, SourceLocation PatternLoc)
      : Class(MPC), PatternLoc(PatternLoc) {}

  explicit MatchPattern() {}

public:
  MatchPatternClass getMatchPatternClass() const {
    return Class; 
  }

  const char *getMatchPatternClassName() const;

  SourceRange getSourceRange() const LLVM_READONLY {
    return {getBeginLoc(), getEndLoc()};
  }

  SourceLocation getBeginLoc() const { return PatternLoc; }
  SourceLocation getEndLoc() const;

  llvm::iterator_range<MatchPattern **> children();

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<MatchPattern *>(this)->children();
  }
};

class WildcardPattern final : public MatchPattern {
public:
  explicit WildcardPattern(SourceLocation WildcardLoc)
      : MatchPattern(WildcardPatternClass, WildcardLoc) {}

  explicit WildcardPattern() {}

  SourceLocation getEndLoc() const { return getBeginLoc(); }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<WildcardPattern *>(this)->children();
  }
};

class ExpressionPattern final : public MatchPattern {
  Expr *SubExpr;

public:
  explicit ExpressionPattern(Expr *SubExpr);

  explicit ExpressionPattern() {}

  SourceLocation getEndLoc() const;

  const Expr *getExpr() const { return SubExpr; }
  Expr *getExpr() { return SubExpr; }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<ExpressionPattern *>(this)->children();
  }
};

class OptionalPattern final : public MatchPattern {
  MatchPattern *SubPattern;

public:
  explicit OptionalPattern(SourceLocation QuestionLoc, MatchPattern *SubPattern)
      : MatchPattern(OptionalPatternClass, QuestionLoc),
        SubPattern(SubPattern) {}

  explicit OptionalPattern() {}

  SourceLocation getEndLoc() const { return SubPattern->getEndLoc(); }

  MatchPattern *getSubPattern() { return SubPattern; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&SubPattern, &SubPattern + 1};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<OptionalPattern *>(this)->children();
  }
};

} // end namespace clang

#endif
