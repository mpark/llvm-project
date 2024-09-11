//===- MatchPattern.h - Classes for representing C++ patterns ---*- C++ -*-===//
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
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TrailingObjects.h"

namespace clang {

class ASTContext;
class Expr;
class BindingDecl;
class DecompositionDecl;

class MatchPattern {
public:
  enum MatchPatternClass {
    WildcardPatternClass,
    ExpressionPatternClass,
    BindingPatternClass,
    OptionalPatternClass,
    DecompositionPatternClass,
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

public:
  // Only allow allocation of Stmts using the allocator in ASTContext
  // or by doing a placement new.
  void *operator new(size_t bytes, const ASTContext &C, unsigned alignment = 8);

  void *operator new(size_t bytes, const ASTContext *C,
                     unsigned alignment = 8) {
    return operator new(bytes, *C, alignment);
  }

  void *operator new(size_t bytes, void *mem) noexcept { return mem; }

  void operator delete(void *, const ASTContext &, unsigned) noexcept {}
  void operator delete(void *, const ASTContext *, unsigned) noexcept {}
  void operator delete(void *, size_t) noexcept {}
  void operator delete(void *, void *) noexcept {}

protected:
  explicit MatchPattern(MatchPatternClass MPC) : Class(MPC) {}

public:
  MatchPatternClass getMatchPatternClass() const { return Class; }

  const char *getMatchPatternClassName() const;

  SourceRange getSourceRange() const LLVM_READONLY {
    return {getBeginLoc(), getEndLoc()};
  }

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  llvm::iterator_range<MatchPattern **> children();

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<MatchPattern *>(this)->children();
  }
};

class WildcardPattern final : public MatchPattern {
  SourceLocation WildcardLoc;

public:
  explicit WildcardPattern(SourceLocation WildcardLoc)
      : MatchPattern(WildcardPatternClass), WildcardLoc(WildcardLoc) {}

  SourceLocation getBeginLoc() const { return WildcardLoc; }
  SourceLocation getEndLoc() const { return WildcardLoc; }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<WildcardPattern *>(this)->children();
  }
};

class ExpressionPattern final : public MatchPattern {
  Expr *E;
  Expr *Cond;

public:
  explicit ExpressionPattern(Expr *E);

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  const Expr *getExpr() const { return E; }
  Expr *getExpr() { return E; }
  const Expr *getCond() const { return Cond; }
  Expr *getCond() { return Cond; }

  void setCond(Expr *Cond) { this->Cond = Cond; }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<ExpressionPattern *>(this)->children();
  }
};

class BindingPattern final : public MatchPattern {
  BindingDecl *Binding;

public:
  explicit BindingPattern(BindingDecl *Binding)
      : MatchPattern(BindingPatternClass), Binding(Binding) {}

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  const BindingDecl *getBinding() const { return Binding; }
  BindingDecl *getBinding() { return Binding; }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<BindingPattern *>(this)->children();
  }
};

class OptionalPattern final : public MatchPattern {
  SourceLocation QuestionLoc;
  MatchPattern *Pattern;
  Expr *Cond;

public:
  explicit OptionalPattern(SourceLocation QuestionLoc, MatchPattern *Pattern)
      : MatchPattern(OptionalPatternClass), QuestionLoc(QuestionLoc),
        Pattern(Pattern) {}

  SourceLocation getBeginLoc() const { return QuestionLoc; }
  SourceLocation getEndLoc() const { return Pattern->getEndLoc(); }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  const Expr *getCond() const { return Cond; }
  Expr *getCond() { return Cond; }

  void setCond(Expr *Cond) { this->Cond = Cond; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&Pattern, &Pattern + 1};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<OptionalPattern *>(this)->children();
  }
};

class DecompositionPattern final
    : public MatchPattern,
      private llvm::TrailingObjects<DecompositionPattern, MatchPattern *> {
  friend class TrailingObjects;

  unsigned NumPatterns;
  SourceRange Squares;
  bool BindingOnly;
  DecompositionDecl *Decomposed;

  explicit DecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares, bool BindingOnly);

  explicit DecompositionPattern(unsigned NumPatterns)
      : MatchPattern(DecompositionPatternClass), NumPatterns(NumPatterns) {}

  const MatchPattern *const *getPatterns() const {
    return getTrailingObjects<MatchPattern *>();
  }

  MatchPattern **getPatterns() { return getTrailingObjects<MatchPattern *>(); }

public:
  unsigned numTrailingObjects(OverloadToken<MatchPattern *>) const {
    return NumPatterns;
  }

  static DecompositionPattern *Create(const ASTContext &Ctx,
                                      ArrayRef<MatchPattern *> Patterns,
                                      SourceRange Squares, bool BindingOnly);

  static DecompositionPattern *CreateEmpty(const ASTContext &Ctx,
                                           unsigned NumPatterns);

  unsigned getNumPatterns() const { return NumPatterns; }

  DecompositionDecl *getDecomposedDecl() const { return Decomposed; }

  void setDecomposedDecl(DecompositionDecl *Decomposed) {
    this->Decomposed = Decomposed;
  }

  SourceLocation getBeginLoc() const { return Squares.getBegin(); }
  SourceLocation getEndLoc() const { return Squares.getEnd(); }
  SourceRange getSquares() const { return Squares; }

  llvm::iterator_range<MatchPattern **> children() {
    return {getPatterns(), getPatterns() + NumPatterns};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<DecompositionPattern *>(this)->children();
  }
};

} // end namespace clang

#endif
