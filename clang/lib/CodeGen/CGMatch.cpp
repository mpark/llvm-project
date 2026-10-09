//===--- CGMatch.cpp - Pattern match decision trees -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGMatch.h"

#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace CodeGen;

namespace {
struct MatchColumn {
  const MatchPattern *Pattern = nullptr;
  const MatchProjection *Discriminator = nullptr;
  unsigned AlternativeIndex = 0;
  SmallVector<const MatchProjection *, 2> ProjectionPath;

  const VarDecl *getIndexVar() const {
    return Discriminator ? Discriminator->getIntermediateVar() : nullptr;
  }
};

struct MatchRow {
  const MatchCaseInstantiation *Case = nullptr;
  SmallVector<MatchColumn, 4> Columns;
  SmallVector<const MatchPattern *, 2> KnownDiscriminators;

  const MatchColumn *find(const VarDecl *IndexVar) const {
    auto It = llvm::find_if(Columns, [&](const MatchColumn &Column) {
      return Column.getIndexVar() == IndexVar;
    });
    return It == Columns.end() ? nullptr : &*It;
  }
};

static void
appendPatternColumns(const MatchPattern *Pattern,
                     const MatchPatternInstantiation *Instantiation,
                     ArrayRef<const MatchProjection *> ProjectionPath,
                     SmallVectorImpl<MatchColumn> &Columns) {
  Pattern = Pattern->IgnoreParens();

  const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern);
  if (!Decomposition) {
    MatchColumn &Column = Columns.emplace_back();
    Column.Pattern = Pattern;
    Column.ProjectionPath.append(ProjectionPath.begin(), ProjectionPath.end());
    const MatchPatternInfo *Info = Instantiation->find(Pattern);
    const MatchProjection *Projection = Info ? Info->Projection : nullptr;
    if (Info && !Info->IsOpenAlternative && Projection &&
        Projection->getKind() == MatchProjection::AlternativeProjection &&
        Projection->getIntermediateVar() &&
        Info->SelectedAlternatives.size() == 1) {
      Column.Discriminator = Projection;
      Column.AlternativeIndex = Info->SelectedAlternatives.front();
    }
    return;
  }

  const MatchPatternInfo *Info = Instantiation->find(Decomposition);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  assert(Projection &&
         Projection->getKind() == MatchProjection::DecompositionProjection &&
         "expected decomposition projection");
  SmallVector<const MatchProjection *, 4> ChildPath(ProjectionPath);
  ChildPath.push_back(Projection);
  for (const MatchPattern *SubPattern :
       Instantiation->getDecompositionPatterns(Decomposition))
    appendPatternColumns(SubPattern, Instantiation, ChildPath, Columns);
}

static MatchRow specializeColumn(const MatchRow &Source,
                                 const MatchColumn *Column) {
  unsigned Index = Column - Source.Columns.begin();
  MatchRow Row = Source;
  Row.KnownDiscriminators.push_back(Column->Pattern);
  Row.Columns.erase(Row.Columns.begin() + Index);

  const auto *Alternative = dyn_cast<AlternativePattern>(Column->Pattern);
  if (!Alternative || !Alternative->getSubPattern())
    return Row;

  const MatchPatternInfo *Info =
      Source.Case->PatternInstantiation->find(Alternative);
  assert(Info && Info->Projection && "expected alternative projection");
  SmallVector<const MatchProjection *, 4> PayloadPath;
  PayloadPath.append(Column->ProjectionPath.begin(),
                     Column->ProjectionPath.end());
  PayloadPath.push_back(Info->Projection);
  appendPatternColumns(Alternative->getSubPattern(),
                       Source.Case->PatternInstantiation, PayloadPath,
                       Row.Columns);
  return Row;
}

static std::unique_ptr<MatchDecisionTree::Node>
buildDecisionTree(ArrayRef<MatchRow> Rows) {
  using Node = MatchDecisionTree::Node;
  if (Rows.empty())
    return std::make_unique<Node>(Node::NK_Failure);

  auto BuildCandidate = [&] {
    auto Result = std::make_unique<Node>(Node::NK_Candidate);
    Result->MatchCandidate = {Rows.front().Case,
                              Rows.front().KnownDiscriminators};
    Result->Failure = buildDecisionTree(Rows.drop_front());
    if (Rows.size() > 1 &&
        Rows[1].Case->CaseIndex == Rows.front().Case->CaseIndex &&
        Rows.front().Case->Guard.hasGuard()) {
      ArrayRef<MatchRow> FollowingCase = Rows.drop_front();
      while (!FollowingCase.empty() && FollowingCase.front().Case->CaseIndex ==
                                           Rows.front().Case->CaseIndex)
        FollowingCase = FollowingCase.drop_front();
      Result->GuardFailure = buildDecisionTree(FollowingCase);
    }
    return Result;
  };

  const MatchRow &First = Rows.front();
  const MatchColumn *Selected = nullptr;
  unsigned SelectedFrequency = 0;
  // Among columns in the first row, choose the one tested by the greatest
  // number of remaining rows. This is Jacobs's duplication heuristic.
  for (const MatchColumn &Column : First.Columns) {
    if (!Column.Discriminator)
      continue;
    const VarDecl *IndexVar = Column.getIndexVar();
    unsigned Frequency = llvm::count_if(Rows, [&](const MatchRow &Row) {
      return Row.find(IndexVar) != nullptr;
    });
    if (Frequency > SelectedFrequency) {
      Selected = &Column;
      SelectedFrequency = Frequency;
    }
  }

  if (!Selected)
    return BuildCandidate();

  const VarDecl *IndexVar = Selected->getIndexVar();
  SmallVector<MatchRow, 8> SuccessRows;
  SmallVector<MatchRow, 8> FailureRows;
  for (const MatchRow &Row : Rows) {
    const MatchColumn *Column = Row.find(IndexVar);
    // Rows without this column apply on both edges. A matching constructor
    // consumes the column on the success edge; a different constructor can
    // only remain on the failure edge.
    if (!Column) {
      SuccessRows.push_back(Row);
      FailureRows.push_back(Row);
    } else if (Column->AlternativeIndex == Selected->AlternativeIndex) {
      SuccessRows.push_back(specializeColumn(Row, Column));
    } else {
      FailureRows.push_back(Row);
    }
  }

  auto Result = std::make_unique<Node>(Node::NK_Test);
  Result->MatchTest.Case = First.Case;
  Result->MatchTest.AlternativeIndex = Selected->AlternativeIndex;
  Result->MatchTest.Discriminator = Selected->Discriminator;
  Result->MatchTest.ProjectionPath = Selected->ProjectionPath;
  Result->Success = buildDecisionTree(SuccessRows);
  Result->Failure = buildDecisionTree(FailureRows);
  return Result;
}

} // namespace

MatchDecisionTree
MatchDecisionTree::build(ArrayRef<MatchCaseInstantiation> Cases) {
  SmallVector<MatchRow, 8> Rows;
  Rows.reserve(Cases.size());
  for (const MatchCaseInstantiation &Case : Cases) {
    MatchRow &Row = Rows.emplace_back();
    Row.Case = &Case;
    appendPatternColumns(Case.Pattern, Case.PatternInstantiation, {},
                         Row.Columns);
  }
  return MatchDecisionTree(buildDecisionTree(Rows));
}
