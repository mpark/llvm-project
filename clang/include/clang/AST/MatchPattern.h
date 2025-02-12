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

#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TrailingObjects.h"

namespace clang {

class ASTContext;
class Expr;
class BindingDecl;
class DecompositionDecl;
class VarDecl;

class MatchPattern {
public:
  enum MatchPatternClass {
    WildcardPatternClass,
    ExpressionPatternClass,
    BindingPatternClass,
    ParenPatternClass,
    OptionalPatternClass,
    AlternativePatternClass,
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
  Expr *Cond = nullptr;

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
  SourceLocation LetLoc;
  BindingDecl *Binding;

public:
  explicit BindingPattern(SourceLocation LetLoc, BindingDecl *Binding)
      : MatchPattern(BindingPatternClass), LetLoc(LetLoc), Binding(Binding) {}

  SourceLocation getLetLoc() const { return LetLoc; }
  SourceLocation getBeginLoc() const { return getLetLoc(); }
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

class ParenPattern final : public MatchPattern {
  SourceRange Parens;
  MatchPattern *Pattern;

public:
  explicit ParenPattern(SourceRange Parens, MatchPattern *Pattern)
      : MatchPattern(ParenPatternClass), Parens(Parens), Pattern(Pattern) {}

  SourceLocation getBeginLoc() const { return Parens.getBegin(); }
  SourceLocation getEndLoc() const { return Parens.getEnd(); }
  SourceRange getParens() const { return Parens; }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&Pattern, &Pattern + 1};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<ParenPattern *>(this)->children();
  }
};

class OptionalPattern final : public MatchPattern {
  SourceLocation QuestionLoc;
  MatchPattern *Pattern;
  VarDecl *CondVar = nullptr;
  Expr *Cond = nullptr;

public:
  explicit OptionalPattern(SourceLocation QuestionLoc, MatchPattern *Pattern)
      : MatchPattern(OptionalPatternClass), QuestionLoc(QuestionLoc),
        Pattern(Pattern) {}

  SourceLocation getBeginLoc() const { return QuestionLoc; }
  SourceLocation getEndLoc() const { return Pattern->getEndLoc(); }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  const VarDecl *getCondVar() const { return CondVar; }
  VarDecl *getCondVar() { return CondVar; }

  void setCondVar(VarDecl *CondVar) { this->CondVar = CondVar; }

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

class AlternativePattern final : public MatchPattern {
  SourceRange TypeRange;
  TypeSourceInfo* TInfo;
  SourceLocation ColonLoc;
  MatchPattern *Pattern;
  VarDecl *HoldingVar = nullptr;
  VarDecl *CondVar = nullptr;
  Expr *Cond = nullptr;
  VarDecl *BindingVar = nullptr;

public:
  explicit AlternativePattern(SourceRange TypeRange, TypeSourceInfo *TInfo,
                              SourceLocation ColonLoc, MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass), TypeRange(TypeRange),
        TInfo(TInfo), ColonLoc(ColonLoc), Pattern(Pattern) {}

  SourceRange getTypeRange() const { return TypeRange; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  SourceLocation getBeginLoc() const { return TypeRange.getBegin(); }
  SourceLocation getEndLoc() const { return Pattern->getEndLoc(); }

  TypeSourceInfo *getTypeSourceInfo() const {
    return TInfo;
  }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  const VarDecl *getHoldingVar() const { return HoldingVar; }
  VarDecl *getHoldingVar() { return HoldingVar; }

  void setHoldingVar(VarDecl *HoldingVar) { this->HoldingVar = HoldingVar; }

  const VarDecl *getCondVar() const { return CondVar; }
  VarDecl *getCondVar() { return CondVar; }

  void setCondVar(VarDecl *CondVar) { this->CondVar = CondVar; }

  const Expr *getCond() const { return Cond; }
  Expr *getCond() { return Cond; }

  void setCond(Expr *Cond) { this->Cond = Cond; }

  const VarDecl *getBindingVar() const { return BindingVar; }
  VarDecl *getBindingVar() { return BindingVar; }

  void setBindingVar(VarDecl *BindingVar) { this->BindingVar = BindingVar; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&Pattern, &Pattern + 1};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<AlternativePattern *>(this)->children();
  }
};

class DecompositionPattern final
    : public MatchPattern,
      private llvm::TrailingObjects<DecompositionPattern, MatchPattern *> {
  friend class TrailingObjects;

  unsigned NumPatterns;
  SourceRange Squares;
  bool BindingOnly;
  DecompositionDecl *Decomposed = nullptr;

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
  bool isBindingOnly() const { return BindingOnly; }

  llvm::iterator_range<MatchPattern **> children() {
    return {getPatterns(), getPatterns() + NumPatterns};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<DecompositionPattern *>(this)->children();
  }
};

} // end namespace clang

#endif
