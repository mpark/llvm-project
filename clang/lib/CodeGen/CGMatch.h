//===--- CGMatch.h - Pattern match decision trees ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CODEGEN_CGMATCH_H
#define LLVM_CLANG_LIB_CODEGEN_CGMATCH_H

#include "clang/AST/ExprCXX.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>
#include <utility>

namespace clang::CodeGen {

bool hasMatchPatternDeclarations(
    const MatchPattern *Pattern,
    const MatchPatternInstantiation *Instantiation);

/// Non-owning access to the checked form consumed by runtime matching.
class MatchCandidateRef {
  llvm::PointerUnion<const MatchCaseInstantiation *,
                     const MatchTestInstantiation *>
      Record;

public:
  MatchCandidateRef() = default;
  MatchCandidateRef(const MatchCaseInstantiation &Case) : Record(&Case) {}
  MatchCandidateRef(const MatchTestInstantiation &Test) : Record(&Test) {}

  const MatchCaseInstantiation *getCase() const {
    return llvm::dyn_cast_if_present<const MatchCaseInstantiation *>(Record);
  }
  const MatchTestInstantiation *getTest() const {
    return llvm::dyn_cast_if_present<const MatchTestInstantiation *>(Record);
  }
  const MatchPattern *getPattern() const {
    return getCase() ? getCase()->Pattern : getTest()->Pattern;
  }
  const MatchPatternInstantiation *getPatternInstantiation() const {
    return getCase() ? getCase()->PatternInstantiation
                     : getTest()->PatternInstantiation;
  }
  const MatchGuard &getGuard() const {
    return getCase() ? getCase()->Guard : getTest()->Guard;
  }
  ArrayRef<const Attr *> getAttributes() const {
    return getCase() ? getCase()->Attributes : ArrayRef<const Attr *>{};
  }
  unsigned getSourceIndex() const {
    return getCase() ? getCase()->CaseIndex : 0;
  }
  friend bool operator==(MatchCandidateRef LHS, MatchCandidateRef RHS) {
    return LHS.Record == RHS.Record;
  }
  friend bool operator!=(MatchCandidateRef LHS, MatchCandidateRef RHS) {
    return !(LHS == RHS);
  }
};

struct MatchPathComponent {
  enum ComponentKind {
    Decomposition,
    ClosedAlternative,
    OpenAlternative,
    Cast
  } Kind;
  const MatchProjection *Projection;
  unsigned Index = 0;
  QualType Type;
};

/// One checked test, shared by matrix columns and emitted decision nodes.
struct MatchDecisionTest {
  enum TestKind {
    Residual,
    ClosedAlternative,
    OpenProjection,
    IntegralValue,
    Constant
  } Kind = Residual;

  MatchCandidateRef Candidate;
  const Expr *Condition = nullptr;
  const MatchProjection *Projection = nullptr;
  const MatchIntegralPatternTest *IntegralTest = nullptr;
  unsigned AlternativeIndex = 0;
  QualType ProjectionType;
  bool ConstantValue = false;
  SmallVector<MatchPathComponent, 2> SubjectPath;
};

struct MatchBindingAction {
  enum ActionKind { Projection, Declaration } Kind;
  const MatchProjection *Project = nullptr;
  const DeclarationPattern *Declare = nullptr;
  SmallVector<MatchPathComponent, 2> SubjectPath;
};

/// Bindings between and-pattern conjuncts are ordering barriers. Other
/// bindings are deferred until all tests in their row have succeeded.
struct MatchOperation {
  enum OperationKind { Test, Bindings } Kind = Test;
  MatchDecisionTest MatchTest;
  SmallVector<MatchBindingAction, 2> BindingActions;
};

struct MatchDecisionCandidate {
  MatchCandidateRef Record;
  SmallVector<MatchOperation, 4> Operations;
  SmallVector<MatchBindingAction, 2> Bindings;
  bool ShareAction = false;
};

/// A decision tree over the refutable columns of a match.
///
/// Specialization consumes tests and exposes projected payloads. Candidate
/// nodes contain explicit remaining operations and deferred bindings, not a
/// source pattern to replay with a list of already completed tests.
class MatchDecisionTree {
public:
  struct Node;

  struct Switch {
    struct Branch {
      llvm::APSInt Value;
      std::unique_ptr<Node> Success;
    };
    SmallVector<Branch, 4> Branches;
  };

  struct BatchedCast {
    const CXXDynamicCastExpr *SourceCast = nullptr;
    const VarDecl *HoldingVar = nullptr;
    struct Target {
      QualType Type;
      std::unique_ptr<Node> Success;
    };
    SmallVector<Target, 4> Targets;
  };

  struct Node {
    enum NodeKind {
      NK_Failure,
      NK_Candidate,
      NK_Test,
      NK_Switch,
      NK_BatchedCast
    } Kind;

    MatchDecisionCandidate MatchCandidate = {};
    MatchDecisionTest MatchTest = {};
    Switch MatchSwitch = {};
    BatchedCast MatchBatchedCast = {};
    std::unique_ptr<Node> Success;
    std::unique_ptr<Node> Failure;
    std::unique_ptr<Node> GuardFailure;

    explicit Node(NodeKind Kind) : Kind(Kind) {}
  };

private:
  std::unique_ptr<Node> Root;

  explicit MatchDecisionTree(std::unique_ptr<Node> Root)
      : Root(std::move(Root)) {}

public:
  static MatchDecisionTree build(ArrayRef<MatchCaseInstantiation> Cases,
                                 const ASTContext &Context,
                                 bool EnableBatchedDynamicCast = false);
  static MatchDecisionTree build(ArrayRef<MatchTestInstantiation> Tests,
                                 const ASTContext &Context,
                                 bool EnableBatchedDynamicCast = false);

  const Node &getRoot() const { return *Root; }
};

} // namespace clang::CodeGen

#endif // LLVM_CLANG_LIB_CODEGEN_CGMATCH_H
