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

static SmallVector<MatchRow, 4>
expandPatternRows(MatchRow Row, const MatchPattern *Pattern,
                  const MatchPatternInstantiation *Instantiation,
                  ArrayRef<const MatchProjection *> ProjectionPath);

static SmallVector<MatchRow, 4>
expandPatternSequence(MatchRow Row, ArrayRef<MatchPattern *> Patterns,
                      const MatchPatternInstantiation *Instantiation,
                      ArrayRef<const MatchProjection *> ProjectionPath) {
  SmallVector<MatchRow, 4> Rows;
  Rows.push_back(std::move(Row));
  for (const MatchPattern *Pattern : Patterns) {
    SmallVector<MatchRow, 4> Expanded;
    for (MatchRow &Current : Rows) {
      SmallVector<MatchRow, 4> PatternRows = expandPatternRows(
          std::move(Current), Pattern, Instantiation, ProjectionPath);
      Expanded.append(std::make_move_iterator(PatternRows.begin()),
                      std::make_move_iterator(PatternRows.end()));
    }
    Rows = std::move(Expanded);
  }
  return Rows;
}

static SmallVector<MatchRow, 4>
expandPatternRows(MatchRow Row, const MatchPattern *Pattern,
                  const MatchPatternInstantiation *Instantiation,
                  ArrayRef<const MatchProjection *> ProjectionPath) {
  Pattern = Pattern->IgnoreParens();

  if (const auto *Or = dyn_cast<OrPattern>(Pattern)) {
    SmallVector<MatchRow, 4> Result;
    for (auto [I, Alternative] : llvm::enumerate(Or->alternatives())) {
      if (!Instantiation->isViableOrAlternative(Or, I))
        continue;
      SmallVector<MatchRow, 4> AlternativeRows =
          expandPatternRows(Row, Alternative, Instantiation, ProjectionPath);
      Result.append(std::make_move_iterator(AlternativeRows.begin()),
                    std::make_move_iterator(AlternativeRows.end()));
    }
    return Result;
  }

  if (const auto *And = dyn_cast<AndPattern>(Pattern))
    return expandPatternRows(std::move(Row), And->conjuncts().front(),
                             Instantiation, ProjectionPath);

  const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern);
  if (!Decomposition) {
    MatchColumn &Column = Row.Columns.emplace_back();
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
    SmallVector<MatchRow, 4> Result;
    Result.push_back(std::move(Row));
    return Result;
  }

  const MatchPatternInfo *Info = Instantiation->find(Decomposition);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  assert(Projection &&
         Projection->getKind() == MatchProjection::DecompositionProjection &&
         "expected decomposition projection");
  SmallVector<const MatchProjection *, 4> ChildPath(ProjectionPath);
  ChildPath.push_back(Projection);
  return expandPatternSequence(
      std::move(Row), Instantiation->getDecompositionPatterns(Decomposition),
      Instantiation, ChildPath);
}

static SmallVector<MatchRow, 4> specializeColumn(const MatchRow &Source,
                                                 const MatchColumn *Column) {
  unsigned Index = Column - Source.Columns.begin();
  MatchRow Row = Source;
  Row.KnownDiscriminators.push_back(Column->Pattern);
  Row.Columns.erase(Row.Columns.begin() + Index);

  const auto *Alternative = dyn_cast<AlternativePattern>(Column->Pattern);
  if (!Alternative || !Alternative->getSubPattern()) {
    SmallVector<MatchRow, 4> Result;
    Result.push_back(std::move(Row));
    return Result;
  }

  const MatchPatternInfo *Info =
      Source.Case->PatternInstantiation->find(Alternative);
  assert(Info && Info->Projection && "expected alternative projection");
  SmallVector<const MatchProjection *, 4> PayloadPath;
  PayloadPath.append(Column->ProjectionPath.begin(),
                     Column->ProjectionPath.end());
  PayloadPath.push_back(Info->Projection);
  return expandPatternRows(std::move(Row), Alternative->getSubPattern(),
                           Source.Case->PatternInstantiation, PayloadPath);
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
    ArrayRef<MatchRow> FollowingPattern = Rows.drop_front();
    while (!FollowingPattern.empty() &&
           FollowingPattern.front().Case == Rows.front().Case)
      FollowingPattern = FollowingPattern.drop_front();
    Result->Failure = buildDecisionTree(FollowingPattern);
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
      SmallVector<MatchRow, 4> Specialized = specializeColumn(Row, Column);
      SuccessRows.append(std::make_move_iterator(Specialized.begin()),
                         std::make_move_iterator(Specialized.end()));
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
    MatchRow Row;
    Row.Case = &Case;
    SmallVector<MatchRow, 4> Expanded = expandPatternRows(
        std::move(Row), Case.Pattern, Case.PatternInstantiation, {});
    Rows.append(std::make_move_iterator(Expanded.begin()),
                std::make_move_iterator(Expanded.end()));
  }
  return MatchDecisionTree(buildDecisionTree(Rows));
}
