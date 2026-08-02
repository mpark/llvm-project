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

#include "clang/AST/ASTConcept.h"
#include "clang/AST/Type.h"
#include "clang/AST/DependenceFlags.h"
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

class MatchProjection {
public:
  enum ProjectionKind {
    OptionalProjection,
    AlternativeProjection,
    CastProjection,
    DecompositionProjection,
  };

private:
  ProjectionKind Kind;
  VarDecl *HoldingVar = nullptr;
  VarDecl *IntermediateVar = nullptr;
  VarDecl *ConditionVar = nullptr;
  VarDecl *ProjectedVar = nullptr;
  DecompositionDecl *DecomposedDecl = nullptr;
  Expr *ConditionExpr = nullptr;
  Expr *ProjectedExpr = nullptr;

public:
  explicit MatchProjection(ProjectionKind Kind) : Kind(Kind) {}

  void *operator new(size_t Bytes, const ASTContext &Context,
                     unsigned Alignment = 8);
  void operator delete(void *, const ASTContext &, unsigned) noexcept {}

  ProjectionKind getKind() const { return Kind; }

  VarDecl *getHoldingVar() const { return HoldingVar; }
  void setHoldingVar(VarDecl *D) { HoldingVar = D; }

  VarDecl *getIntermediateVar() const { return IntermediateVar; }
  void setIntermediateVar(VarDecl *D) { IntermediateVar = D; }

  VarDecl *getConditionVar() const { return ConditionVar; }
  void setConditionVar(VarDecl *D) { ConditionVar = D; }

  VarDecl *getProjectedVar() const { return ProjectedVar; }
  void setProjectedVar(VarDecl *D) { ProjectedVar = D; }

  DecompositionDecl *getDecomposedDecl() const { return DecomposedDecl; }
  void setDecomposedDecl(DecompositionDecl *D) { DecomposedDecl = D; }

  Expr *getConditionExpr() const { return ConditionExpr; }
  void setConditionExpr(Expr *E) { ConditionExpr = E; }

  Expr *getProjectedExpr() const { return ProjectedExpr; }
  void setProjectedExpr(Expr *E) { ProjectedExpr = E; }
};

class MatchPattern {
public:
  enum MatchPatternClass {
    WildcardPatternClass,
    ExpressionPatternClass,
    BindingPatternClass,
    ParenPatternClass,
    DeclarationPatternClass,
    TypePatternClass,
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
  ExprDependence Dependent;

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

  void setDependence(ExprDependence D) { Dependent = D; }
public:
  MatchPatternClass getMatchPatternClass() const { return Class; }

  const char *getMatchPatternClassName() const;

  ExprDependence computeDependence() {
    ExprDependence Dependent = ExprDependence::None;
    for (const MatchPattern* C : children()) {
      Dependent |= C->getDependence();
    }
    return Dependent;
  }

  ExprDependence getDependence() const {
    return Dependent;
  }

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
      : MatchPattern(WildcardPatternClass), WildcardLoc(WildcardLoc) {
    setDependence(ExprDependence::None);
  }

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
  bool IsPackExpansion;

public:
  explicit ExpressionPattern(Expr *E, bool IsPackExpansion);

  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == ExpressionPatternClass;
  }

  bool isPackExpansion() const { return IsPackExpansion; };

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
      : MatchPattern(BindingPatternClass), LetLoc(LetLoc), Binding(Binding) {
    setDependence(ExprDependence::None);
  }

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
      : MatchPattern(ParenPatternClass), Parens(Parens), Pattern(Pattern) {
    setDependence(computeDependence());
  }

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

class DeclarationPattern final : public MatchPattern {
  VarDecl *Declaration;
  SourceRange DeclarationRange;
  MatchProjection *Projection = nullptr;

public:
  explicit DeclarationPattern(VarDecl *Declaration, SourceRange WrittenRange);

  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == DeclarationPatternClass;
  }

  const VarDecl *getDeclaration() const { return Declaration; }
  VarDecl *getDeclaration() { return Declaration; }

  MatchProjection *getProjection() const { return Projection; }
  void setProjection(MatchProjection *P) { Projection = P; }

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<DeclarationPattern *>(this)->children();
  }
};

class TypePattern final : public MatchPattern {
  TypeSourceInfo *TInfo;
  MatchProjection *Projection = nullptr;
  QualType SubjectType;
  bool Resolved = false;
  bool Matches = false;

public:
  explicit TypePattern(TypeSourceInfo *TInfo)
      : MatchPattern(TypePatternClass), TInfo(TInfo) {
    setDependence(
        toExprDependenceForImpliedType(TInfo->getType()->getDependence()));
  }

  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == TypePatternClass;
  }

  TypeSourceInfo *getTypeSourceInfo() const { return TInfo; }
  QualType getType() const { return TInfo->getType(); }

  MatchProjection *getProjection() const { return Projection; }
  void setProjection(MatchProjection *P) { Projection = P; }

  bool isResolved() const { return Resolved; }
  bool matches() const {
    assert(Resolved && "unresolved type pattern");
    return Matches;
  }
  QualType getSubjectType() const {
    assert(Resolved && "unresolved type pattern");
    return SubjectType;
  }
  void setMatches(bool Value, QualType CheckedSubjectType) {
    Resolved = true;
    Matches = Value;
    SubjectType = CheckedSubjectType;
  }

  SourceLocation getBeginLoc() const {
    return TInfo->getTypeLoc().getBeginLoc();
  }
  SourceLocation getEndLoc() const { return TInfo->getTypeLoc().getEndLoc(); }

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<TypePattern *>(this)->children();
  }
};

class OptionalPattern final : public MatchPattern {
  SourceLocation QuestionLoc;
  MatchPattern *Pattern;
  MatchProjection *Projection = nullptr;

public:
  explicit OptionalPattern(SourceLocation QuestionLoc, MatchPattern *Pattern)
      : MatchPattern(OptionalPatternClass), QuestionLoc(QuestionLoc),
        Pattern(Pattern) {
    setDependence(computeDependence());
  }

  SourceLocation getBeginLoc() const { return QuestionLoc; }
  SourceLocation getEndLoc() const { return Pattern->getEndLoc(); }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  MatchProjection *getProjection() const { return Projection; }
  void setProjection(MatchProjection *P) { Projection = P; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&Pattern, &Pattern + 1};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<OptionalPattern *>(this)->children();
  }
};

class AlternativePattern final : public MatchPattern {
  SourceRange DiscriminatorRange;

  // Discriminator is either a type or a type-constraint.
  TypeSourceInfo* TInfo = nullptr;
  ConceptReference* CR = nullptr;

  SourceLocation ColonLoc;
  MatchPattern *Pattern;
  MatchProjection *Projection = nullptr;
  bool IsAuto = false;

public:
  explicit AlternativePattern(SourceRange ConceptRange, ConceptReference *CR,
                              SourceLocation ColonLoc, MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass), DiscriminatorRange(ConceptRange),
        CR(CR), ColonLoc(ColonLoc), Pattern(Pattern) {
    // FIXME(mpark): Account for ConceptReference.
    setDependence(computeDependence());
  }

  explicit AlternativePattern(SourceRange TypeRange, TypeSourceInfo *TInfo,
                              SourceLocation ColonLoc, MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass), DiscriminatorRange(TypeRange),
        TInfo(TInfo), ColonLoc(ColonLoc), Pattern(Pattern) {
     setDependence(
        toExprDependenceForImpliedType(TInfo->getType()->getDependence()) |
        computeDependence());
  }

  explicit AlternativePattern(SourceRange AutoRange, SourceLocation ColonLoc,
                              MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass),
        DiscriminatorRange(AutoRange), ColonLoc(ColonLoc), Pattern(Pattern),
        IsAuto(true) {
    setDependence(computeDependence());
  }

  SourceRange getDiscriminatorRange() const { return DiscriminatorRange; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  SourceLocation getBeginLoc() const { return DiscriminatorRange.getBegin(); }
  SourceLocation getEndLoc() const { return Pattern->getEndLoc(); }

  ConceptReference *getConceptReference() const {
    return CR;
  }

  TypeSourceInfo *getTypeSourceInfo() const {
    return TInfo;
  }

  bool isAuto() const { return IsAuto; }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  MatchProjection *getProjection() const { return Projection; }
  void setProjection(MatchProjection *P) { Projection = P; }

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
  MatchProjection *Projection = nullptr;

  explicit DecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares, bool BindingOnly);

  explicit DecompositionPattern(unsigned NumPatterns)
      : MatchPattern(DecompositionPatternClass), NumPatterns(NumPatterns) {}

  const MatchPattern *const *getPatterns() const {
    return getTrailingObjects();
  }

  MatchPattern **getPatterns() { return getTrailingObjects(); }

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

  MatchProjection *getProjection() const { return Projection; }
  void setProjection(MatchProjection *P) { Projection = P; }

  DecompositionDecl *getDecomposedDecl() const {
    return Projection ? Projection->getDecomposedDecl() : nullptr;
  }

  void setDecomposedDecl(DecompositionDecl *Decomposed) {
    Projection->setDecomposedDecl(Decomposed);
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
