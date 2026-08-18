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
#include "clang/AST/DependenceFlags.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TrailingObjects.h"

namespace clang {

class ASTContext;
class Decl;
class Expr;
class Stmt;
class BindingDecl;
class DecompositionDecl;
class VarDecl;

class MatchProjection {
public:
  enum ProjectionKind {
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
    DeclarationPatternClass,
    TypePatternClass,
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
  SourceLocation EllipsisLoc;

public:
  explicit WildcardPattern(SourceLocation WildcardLoc,
                           SourceLocation EllipsisLoc = {})
      : MatchPattern(WildcardPatternClass), WildcardLoc(WildcardLoc),
        EllipsisLoc(EllipsisLoc) {
    setDependence(ExprDependence::None);
  }

  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == WildcardPatternClass;
  }

  bool isPackExpansion() const { return EllipsisLoc.isValid(); }

  SourceLocation getBeginLoc() const {
    return isPackExpansion() ? EllipsisLoc : WildcardLoc;
  }
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

  llvm::iterator_range<MatchPattern **> children() {
    return {nullptr, nullptr};
  }

  llvm::iterator_range<const MatchPattern *const *> children() const {
    return const_cast<ExpressionPattern *>(this)->children();
  }
};

class DeclarationPattern final : public MatchPattern {
  VarDecl *Declaration;
  VarDecl *PackSourceDeclaration;
  SourceRange DeclarationRange;

public:
  explicit DeclarationPattern(VarDecl *Declaration, SourceRange WrittenRange,
                              VarDecl *PackSourceDeclaration = nullptr);

  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == DeclarationPatternClass;
  }

  const VarDecl *getDeclaration() const { return Declaration; }
  VarDecl *getDeclaration() { return Declaration; }
  VarDecl *getPackSourceDeclaration() const { return PackSourceDeclaration; }

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

class AlternativePattern final : public MatchPattern {
public:
  enum AlternativeKind { Generic, Named, Empty };

private:
  AlternativeKind Kind;
  SourceRange DiscriminatorRange;
  SourceRange Braces;

  IdentifierInfo *Name = nullptr;

  SourceLocation ColonLoc;
  MatchPattern *Pattern = nullptr;

public:
  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == AlternativePatternClass;
  }

  explicit AlternativePattern(SourceRange Braces, MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass), Kind(Generic), Braces(Braces),
        Pattern(Pattern) {
    setDependence(computeDependence());
  }

  explicit AlternativePattern(SourceRange Braces, SourceRange NameRange,
                              IdentifierInfo *Name, SourceLocation ColonLoc,
                              MatchPattern *Pattern)
      : MatchPattern(AlternativePatternClass), Kind(Named),
        DiscriminatorRange(NameRange), Braces(Braces), Name(Name),
        ColonLoc(ColonLoc), Pattern(Pattern) {
    setDependence(computeDependence());
  }

  explicit AlternativePattern(SourceRange Braces)
      : MatchPattern(AlternativePatternClass), Kind(Empty), Braces(Braces) {
    setDependence(ExprDependence::None);
  }

  AlternativeKind getAlternativeKind() const { return Kind; }
  bool isNamed() const { return Kind == Named; }
  bool isEmpty() const { return Kind == Empty; }

  SourceRange getDiscriminatorRange() const { return DiscriminatorRange; }
  SourceRange getBraces() const { return Braces; }
  IdentifierInfo *getName() const { return Name; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  SourceLocation getBeginLoc() const { return Braces.getBegin(); }
  SourceLocation getEndLoc() const { return Braces.getEnd(); }

  const MatchPattern *getSubPattern() const { return Pattern; }
  MatchPattern *getSubPattern() { return Pattern; }

  llvm::iterator_range<MatchPattern **> children() {
    return {&Pattern, &Pattern + (Pattern != nullptr)};
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
  explicit DecompositionPattern(ArrayRef<MatchPattern *> Patterns,
                                SourceRange Squares);

  explicit DecompositionPattern(unsigned NumPatterns)
      : MatchPattern(DecompositionPatternClass), NumPatterns(NumPatterns) {}

  const MatchPattern *const *getPatterns() const {
    return getTrailingObjects();
  }

  MatchPattern **getPatterns() { return getTrailingObjects(); }

public:
  static bool classof(const MatchPattern *P) {
    return P->getMatchPatternClass() == DecompositionPatternClass;
  }

  unsigned numTrailingObjects(OverloadToken<MatchPattern *>) const {
    return NumPatterns;
  }

  static DecompositionPattern *Create(const ASTContext &Ctx,
                                      ArrayRef<MatchPattern *> Patterns,
                                      SourceRange Squares);

  static DecompositionPattern *CreateEmpty(const ASTContext &Ctx,
                                           unsigned NumPatterns);

  unsigned getNumPatterns() const { return NumPatterns; }

  ArrayRef<MatchPattern *> patterns() { return {getPatterns(), NumPatterns}; }
  ArrayRef<MatchPattern *> patterns() const {
    return {const_cast<MatchPattern **>(getPatterns()), NumPatterns};
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

/// Semantic state produced while checking one instantiation of a pattern.
///
/// MatchPattern nodes describe source syntax and can be shared by multiple
/// template or alternative instantiations. The expressions and declarations
/// used to execute a particular instantiation belong here instead.
struct MatchPatternInfo {
  MatchPattern *Pattern = nullptr;
  Expr *Condition = nullptr;
  MatchProjection *Projection = nullptr;
  QualType CheckedSubjectType;
  QualType AlternativeProviderType;
  ArrayRef<QualType> AlternativeTypes;
  ArrayRef<unsigned char> ProjectableAlternatives;
  ArrayRef<unsigned> SelectedAlternatives;
  ArrayRef<MatchPattern *> ExpandedPatterns;
  QualType OpenAlternativeType;
  bool IsExhaustive = true;
  bool IsOpenAlternative = false;
  bool OpenAlternativeHasEmpty = false;
  bool OpenAlternativeProjectableWildcard = false;
  bool TypePatternResolved = false;
  bool TypePatternMatches = false;
  bool HasExpandedPatterns = false;
};

class MatchPatternInstantiation final
    : private llvm::TrailingObjects<MatchPatternInstantiation,
                                    MatchPatternInfo> {
  friend class llvm::TrailingObjects<MatchPatternInstantiation,
                                     MatchPatternInfo>;

  MatchPattern *Pattern;
  unsigned NumInfos;

  MatchPatternInstantiation(MatchPattern *Pattern, unsigned NumInfos)
      : Pattern(Pattern), NumInfos(NumInfos) {}

public:
  unsigned numTrailingObjects(OverloadToken<MatchPatternInfo>) const {
    return NumInfos;
  }

  static MatchPatternInstantiation *Create(const ASTContext &Ctx,
                                           MatchPattern *Pattern,
                                           ArrayRef<MatchPatternInfo> Infos);

  MatchPattern *getPattern() const { return Pattern; }
  ArrayRef<MatchPatternInfo> infos() const {
    return ArrayRef(getTrailingObjects(), NumInfos);
  }
  const MatchPatternInfo *find(const MatchPattern *P) const;
  ArrayRef<MatchPattern *>
  getDecompositionPatterns(const DecompositionPattern *P) const;
};

/// Visit the declarations and statements that implement one semantically
/// checked pattern. Repeated pointers within the instantiation are reported
/// once.
void visitMatchPatternEvaluation(
    const MatchPattern *Pattern, const MatchPatternInstantiation *Instantiation,
    llvm::function_ref<void(const Decl *)> VisitDecl,
    llvm::function_ref<void(const Stmt *)> VisitStmt);

} // end namespace clang

#endif
