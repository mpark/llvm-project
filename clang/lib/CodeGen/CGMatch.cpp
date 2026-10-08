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

const VarDecl *MatchDecisionTree::Discriminator::getIndexVar() const {
  const MatchPatternInfo *Info = Case->PatternInstantiation->find(Pattern);
  assert(Info && Info->Projection &&
         Info->Projection->getIntermediateVar() &&
         "discriminator has no index variable");
  return Info->Projection->getIntermediateVar();
}

static const MatchPattern *
getDirectDiscriminatorPattern(const MatchPattern *Pattern,
                              const MatchPatternInstantiation *Instantiation) {
  while (const auto *Paren = dyn_cast<ParenPattern>(Pattern))
    Pattern = Paren->getSubPattern();
  const MatchPatternInfo *Info = Instantiation->find(Pattern);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  if (!Info || Info->IsOpenAlternative || !Projection ||
      Projection->getKind() != MatchProjection::AlternativeProjection ||
      !Projection->getIntermediateVar() ||
      Info->SelectedAlternatives.size() != 1)
    return nullptr;
  return Pattern;
}

namespace {
struct MatchColumn {
  const MatchPattern *Pattern;
  const MatchProjection *Projection;
  unsigned AlternativeIndex;
};

struct MatchRow {
  const MatchCaseInstantiation *Case;
  SmallVector<MatchColumn, 4> Columns;
  SmallVector<const MatchPattern *, 2> KnownDiscriminators;

  const MatchColumn *find(const VarDecl *IndexVar) const {
    auto It = llvm::find_if(Columns, [&](const MatchColumn &Column) {
      return Column.Projection->getIntermediateVar() == IndexVar;
    });
    return It == Columns.end() ? nullptr : &*It;
  }
};

static void appendDiscriminator(
    SmallVectorImpl<MatchDecisionTree::Discriminator> &Discriminators,
    const MatchCaseInstantiation &Case, const MatchColumn &Column) {
  MatchDecisionTree::Discriminator Discriminator = {&Case, Column.Pattern};
  if (llvm::none_of(Discriminators, [&](const auto &Existing) {
        return Existing.getIndexVar() == Discriminator.getIndexVar();
      }))
    Discriminators.push_back(Discriminator);
}

static void
collectDirectDiscriminators(const MatchPattern *Pattern,
                            const MatchPatternInstantiation *Instantiation,
                            SmallVectorImpl<MatchColumn> &Columns) {
  while (const auto *Paren = dyn_cast<ParenPattern>(Pattern))
    Pattern = Paren->getSubPattern();

  if (const MatchPattern *Discriminator =
          getDirectDiscriminatorPattern(Pattern, Instantiation)) {
    const MatchPatternInfo *Info = Instantiation->find(Discriminator);
    Columns.push_back(
        {Discriminator, Info->Projection, Info->SelectedAlternatives.front()});
    return;
  }

  const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern);
  if (!Decomposition)
    return;
  for (const MatchPattern *SubPattern :
       Instantiation->getDecompositionPatterns(Decomposition))
    collectDirectDiscriminators(SubPattern, Instantiation, Columns);
}

static MatchRow specializeColumn(
    const MatchRow &Source, const MatchColumn *Column,
    SmallVectorImpl<MatchDecisionTree::Discriminator> &NewDiscriminators) {
  unsigned Index = Column - Source.Columns.begin();
  MatchRow Row = Source;
  Row.KnownDiscriminators.push_back(Column->Pattern);
  Row.Columns.erase(Row.Columns.begin() + Index);

  const auto *Alternative = dyn_cast<AlternativePattern>(Column->Pattern);
  if (!Alternative || !Alternative->getSubPattern())
    return Row;

  size_t FirstNewColumn = Row.Columns.size();
  collectDirectDiscriminators(Alternative->getSubPattern(),
                              Source.Case->PatternInstantiation, Row.Columns);
  for (const MatchColumn &NewColumn :
       ArrayRef(Row.Columns).drop_front(FirstNewColumn))
    appendDiscriminator(NewDiscriminators, *Source.Case, NewColumn);
  return Row;
}

static std::unique_ptr<MatchDecisionTree::Node>
buildDecisionTree(ArrayRef<MatchRow> Rows) {
  using Node = MatchDecisionTree::Node;
  if (Rows.empty())
    return std::make_unique<Node>(Node::NK_Failure);

  if (Rows.front().Columns.empty()) {
    auto Result = std::make_unique<Node>(Node::NK_Candidate);
    Result->MatchCandidate = {Rows.front().Case,
                              Rows.front().KnownDiscriminators};
    Result->Failure = buildDecisionTree(Rows.drop_front());
    if (Rows.size() > 1 &&
        Rows[1].Case->CaseIndex == Rows.front().Case->CaseIndex &&
        Rows.front().Case->Guard.hasGuard()) {
      ArrayRef<MatchRow> FollowingCase = Rows.drop_front();
      while (!FollowingCase.empty() &&
             FollowingCase.front().Case->CaseIndex ==
                 Rows.front().Case->CaseIndex)
        FollowingCase = FollowingCase.drop_front();
      Result->GuardFailure = buildDecisionTree(FollowingCase);
    }
    return Result;
  }

  const MatchRow &First = Rows.front();
  const MatchColumn *Selected = nullptr;
  unsigned SelectedFrequency = 0;
  // Among columns in the first row, choose the one tested by the greatest
  // number of remaining rows. This is Jacobs's duplication heuristic.
  for (const MatchColumn &Column : First.Columns) {
    const VarDecl *IndexVar = Column.Projection->getIntermediateVar();
    unsigned Frequency = llvm::count_if(Rows, [&](const MatchRow &Row) {
      return Row.find(IndexVar) != nullptr;
    });
    if (Frequency > SelectedFrequency) {
      Selected = &Column;
      SelectedFrequency = Frequency;
    }
  }

  assert(Selected && "first row has no discriminator");
  const VarDecl *IndexVar = Selected->Projection->getIntermediateVar();
  SmallVector<MatchRow, 8> SuccessRows;
  SmallVector<MatchRow, 8> FailureRows;
  SmallVector<MatchDecisionTree::Discriminator, 2> SuccessDiscriminators;
  for (const MatchRow &Row : Rows) {
    const MatchColumn *Column = Row.find(IndexVar);
    // Rows without this column apply on both edges. A matching constructor
    // consumes the column on the success edge; a different constructor can
    // only remain on the failure edge.
    if (!Column) {
      SuccessRows.push_back(Row);
      FailureRows.push_back(Row);
    } else if (Column->AlternativeIndex == Selected->AlternativeIndex) {
      SuccessRows.push_back(
          specializeColumn(Row, Column, SuccessDiscriminators));
    } else {
      FailureRows.push_back(Row);
    }
  }

  auto Result = std::make_unique<Node>(Node::NK_Test);
  Result->MatchTest.Case = First.Case;
  Result->MatchTest.AlternativeIndex = Selected->AlternativeIndex;
  Result->MatchTest.ConditionProjection = Selected->Projection;
  Result->Success = buildDecisionTree(SuccessRows);
  Result->Success->EntryDiscriminators = std::move(SuccessDiscriminators);
  Result->Failure = buildDecisionTree(FailureRows);
  return Result;
}

} // namespace

std::optional<MatchDecisionTree>
MatchDecisionTree::build(ArrayRef<MatchCaseInstantiation> Cases) {
  if (Cases.empty())
    return std::nullopt;

  SmallVector<MatchRow, 8> Rows;
  Rows.reserve(Cases.size());
  for (const MatchCaseInstantiation &Case : Cases) {
    MatchRow &Row = Rows.emplace_back();
    Row.Case = &Case;
    collectDirectDiscriminators(Case.Pattern, Case.PatternInstantiation,
                                Row.Columns);
    for (unsigned I = 0; I != Row.Columns.size(); ++I) {
      const MatchColumn &Column = Row.Columns[I];
      if (llvm::any_of(ArrayRef(Row.Columns).take_front(I),
                       [&](const MatchColumn &Previous) {
                         return Previous.Projection->getIntermediateVar() ==
                                Column.Projection->getIntermediateVar();
                       }))
        return std::nullopt;
    }
  }

  if (Rows.front().Columns.empty())
    return std::nullopt;

  SmallVector<Discriminator, 4> Discriminators;
  for (const MatchRow &Row : Rows) {
    for (const MatchColumn &Column : Row.Columns)
      appendDiscriminator(Discriminators, *Row.Case, Column);
  }
  std::unique_ptr<Node> Root = buildDecisionTree(Rows);
  Root->EntryDiscriminators = std::move(Discriminators);
  return MatchDecisionTree(std::move(Root));
}
