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
#include "llvm/ADT/SmallVector.h"

#include <memory>
#include <optional>
#include <utility>

namespace clang::CodeGen {

bool hasMatchPatternDeclarations(
    const MatchPattern *Pattern,
    const MatchPatternInstantiation *Instantiation);

/// A decision tree over the refutable columns of a match.
///
/// Test nodes specialize source-ordered rows on closed-alternative states or
/// open-protocol projection conditions. Pattern-test nodes evaluate residual
/// expression and type conditions. A successful test can expose additional
/// columns from a decomposition or projected payload. Candidate nodes hand any
/// remaining tests and the action back to the ordinary match emitter.
class MatchDecisionTree {
public:
  struct Candidate {
    const MatchCaseInstantiation *Case = nullptr;
    SmallVector<const MatchPattern *, 2> KnownTestPatterns;
    SmallVector<const MatchPattern *, 2> CompletedPatterns;
    bool ShareAction = false;
  };

  struct Test {
    const MatchCaseInstantiation *Case = nullptr;
    std::optional<unsigned> AlternativeIndex;
    const MatchProjection *Projection = nullptr;
    /// Ancestor projections needed to reach the test projection.
    SmallVector<const MatchProjection *, 2> ProjectionPath;
  };

  struct PatternTest {
    const MatchCaseInstantiation *Case = nullptr;
    const MatchPattern *Pattern = nullptr;
    /// Ancestor projections needed to reach the pattern subject.
    SmallVector<const MatchProjection *, 2> ProjectionPath;
  };

  struct Node {
    enum NodeKind { NK_Failure, NK_Candidate, NK_Test, NK_PatternTest } Kind;

    Candidate MatchCandidate = {};
    Test MatchTest = {};
    PatternTest ResidualTest = {};
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
  static MatchDecisionTree build(ArrayRef<MatchCaseInstantiation> Cases);

  const Node &getRoot() const { return *Root; }
};

} // namespace clang::CodeGen

#endif // LLVM_CLANG_LIB_CODEGEN_CGMATCH_H
