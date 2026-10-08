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

/// A decision tree over the closed-alternative columns of a match.
///
/// Test nodes specialize the source-ordered rows on one alternative state.
/// A successful test can expose additional columns from the projected payload.
/// Candidate nodes hand the remaining pattern tests and action back to the
/// ordinary match emitter once the first row has no more decision columns.
class MatchDecisionTree {
public:
  struct Discriminator {
    const MatchCaseInstantiation *Case = nullptr;
    const MatchPattern *Pattern = nullptr;

    const VarDecl *getIndexVar() const;
  };

  struct Candidate {
    const MatchCaseInstantiation *Case = nullptr;
    SmallVector<const MatchPattern *, 2> KnownDiscriminatorPatterns;
  };

  struct Test {
    const MatchCaseInstantiation *Case = nullptr;
    unsigned AlternativeIndex = 0;
    const MatchProjection *ConditionProjection = nullptr;
  };

  struct Node {
    enum NodeKind { NK_Failure, NK_Candidate, NK_Test } Kind;

    SmallVector<Discriminator, 2> EntryDiscriminators;
    Candidate MatchCandidate = {};
    Test MatchTest = {};
    std::unique_ptr<Node> Success;
    std::unique_ptr<Node> Failure;

    explicit Node(NodeKind Kind) : Kind(Kind) {}
  };

private:
  std::unique_ptr<Node> Root;

  explicit MatchDecisionTree(std::unique_ptr<Node> Root)
      : Root(std::move(Root)) {}

public:
  static std::optional<MatchDecisionTree>
  build(ArrayRef<MatchCaseInstantiation> Cases);

  const Node &getRoot() const { return *Root; }
};

} // namespace clang::CodeGen

#endif // LLVM_CLANG_LIB_CODEGEN_CGMATCH_H
