//===--- CGMatch.cpp - Pattern match decision trees -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGMatch.h"

#include "clang/AST/ASTContext.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/STLExtras.h"

#include <optional>

using namespace clang;
using namespace CodeGen;

namespace {
struct MatchPathComponent {
  enum ComponentKind { Decomposition, ClosedAlternative, OpenAlternative } Kind;
  const MatchProjection *Projection = nullptr;
  unsigned Index = 0;
  QualType Type;
};

struct MatchColumn {
  enum ColumnKind {
    Residual,
    ClosedAlternative,
    OpenProjection,
    IntegralValue
  } Kind = Residual;

  const MatchPattern *Pattern = nullptr;
  const MatchProjection *Projection = nullptr;
  const BinaryOperator *IntegralComparison = nullptr;
  llvm::APSInt IntegralConstructor;
  unsigned AlternativeIndex = 0;
  QualType ProjectionType;
  bool MaterializeProjection = false;
  SmallVector<MatchPathComponent, 2> SubjectPath;
};

struct MatchRow {
  const MatchCaseInstantiation *Case = nullptr;
  SmallVector<MatchColumn, 4> Columns;
  SmallVector<const MatchPattern *, 2> KnownTests;
  SmallVector<const MatchPattern *, 2> CompletedPatterns;
  SmallVector<const MatchPattern *, 2> CompletedWhenExhausted;
  const MatchPattern *SequencePattern = nullptr;
  const MatchPattern *ActiveConjunct = nullptr;
  SmallVector<const MatchPattern *, 2> RemainingConjuncts;
  bool AllowResidualTests = true;
  bool NeedsSharedAction = false;
};

static SmallVector<MatchRow, 4> singleRow(MatchRow Row) {
  SmallVector<MatchRow, 4> Result;
  Result.push_back(std::move(Row));
  return Result;
}

static bool hasSameTestSubject(const MatchColumn &LHS, const MatchColumn &RHS,
                               const ASTContext &Context) {
  if (LHS.SubjectPath.size() != RHS.SubjectPath.size())
    return false;
  for (auto [L, R] : llvm::zip_equal(LHS.SubjectPath, RHS.SubjectPath)) {
    if (L.Kind != R.Kind || L.Index != R.Index ||
        L.Type.isNull() != R.Type.isNull() ||
        (!L.Type.isNull() && !Context.hasSameType(L.Type, R.Type)))
      return false;
  }

  if (LHS.Kind != RHS.Kind)
    return false;
  switch (LHS.Kind) {
  case MatchColumn::Residual:
    return false;
  case MatchColumn::ClosedAlternative:
    return true;
  case MatchColumn::OpenProjection:
    if (LHS.Projection->getKind() != RHS.Projection->getKind())
      return false;
    if (!LHS.ProjectionType.isNull() || !RHS.ProjectionType.isNull())
      return !LHS.ProjectionType.isNull() && !RHS.ProjectionType.isNull() &&
             Context.hasSameType(LHS.ProjectionType, RHS.ProjectionType);
    return LHS.Projection == RHS.Projection;
  case MatchColumn::IntegralValue:
    return Context.hasSameType(LHS.IntegralComparison->getLHS()->getType(),
                               RHS.IntegralComparison->getLHS()->getType());
  }
  llvm_unreachable("unknown match column kind");
}

static const MatchColumn *findTestColumn(const MatchRow &Row,
                                         const MatchColumn &Test,
                                         const ASTContext &Context) {
  auto It = llvm::find_if(Row.Columns, [&](const MatchColumn &Column) {
    return hasSameTestSubject(Column, Test, Context);
  });
  return It == Row.Columns.end() ? nullptr : &*It;
}

static bool hasSameConstructor(const MatchColumn &LHS, const MatchColumn &RHS) {
  assert(LHS.Kind == RHS.Kind && LHS.Kind != MatchColumn::Residual &&
         "expected columns from the same test family");
  switch (LHS.Kind) {
  case MatchColumn::Residual:
    llvm_unreachable("residual columns are not constructors");
  case MatchColumn::ClosedAlternative:
    return LHS.AlternativeIndex == RHS.AlternativeIndex;
  case MatchColumn::OpenProjection:
    return true;
  case MatchColumn::IntegralValue:
    return llvm::APSInt::isSameValue(LHS.IntegralConstructor,
                                     RHS.IntegralConstructor);
  }
  llvm_unreachable("unknown match column kind");
}

static SmallVector<const MatchProjection *, 2>
getProjectionPath(ArrayRef<MatchPathComponent> SubjectPath) {
  SmallVector<const MatchProjection *, 2> Result;
  for (const MatchPathComponent &Component : SubjectPath) {
    assert(Component.Projection && "subject path component has no projection");
    Result.push_back(Component.Projection);
  }
  return Result;
}

struct IntegralPatternTest {
  const BinaryOperator *Comparison;
  llvm::APSInt Value;
};

static std::optional<IntegralPatternTest>
getIntegralPatternTest(const MatchPattern *Pattern, const MatchRow &Row,
                       const ASTContext &Context) {
  if (!isa<ExpressionPattern>(Pattern) ||
      Stmt::getLikelihood(Row.Case->Attributes) != Stmt::LH_None)
    return std::nullopt;

  const MatchPatternInfo *Info = Row.Case->PatternInstantiation->find(Pattern);
  if (!Info || !Info->Condition)
    return std::nullopt;
  const auto *Comparison =
      dyn_cast<BinaryOperator>(Info->Condition->IgnoreParenImpCasts());
  if (!Comparison || Comparison->getOpcode() != BO_EQ ||
      !Comparison->getLHS()->getType()->isIntegralOrEnumerationType())
    return std::nullopt;

  const Expr *Subject = Comparison->getLHS()->IgnoreParenImpCasts();
  if (Subject->getType().isVolatileQualified())
    return std::nullopt;
  const Expr *ValueExpr = Comparison->getRHS();
  while (true) {
    if (const auto *Parens = dyn_cast<ParenExpr>(ValueExpr))
      ValueExpr = Parens->getSubExpr();
    else if (const auto *Cast = dyn_cast<ImplicitCastExpr>(ValueExpr))
      ValueExpr = Cast->getSubExpr();
    else
      break;
  }
  const auto *Value = dyn_cast<ConstantExpr>(ValueExpr);
  if (!Value || !Value->hasAPValueResult() ||
      !Value->getAPValueResult().isInt())
    return std::nullopt;
  llvm::APSInt Converted = Value->getAPValueResult().getInt().extOrTrunc(
      Context.getIntWidth(Comparison->getRHS()->getType()));
  Converted.setIsUnsigned(
      Comparison->getRHS()->getType()->isUnsignedIntegerOrEnumerationType());
  return IntegralPatternTest{Comparison, Converted};
}

static const MatchProjection *
getDecisionProjection(const MatchPattern *Pattern,
                      const MatchPatternInstantiation *Instantiation) {
  const MatchPatternInfo *Info = Instantiation->find(Pattern);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  if (!Info || !Projection || !Projection->getIntermediateVar())
    return nullptr;
  if (Projection->getKind() == MatchProjection::CastProjection)
    return isa<TypePattern, DeclarationPattern>(Pattern) ? Projection : nullptr;
  if (Projection->getKind() == MatchProjection::AlternativeProjection &&
      (Info->IsOpenAlternative || Info->SelectedAlternatives.size() == 1))
    return Projection;
  return nullptr;
}

static bool
hasPatternDeclarationsImpl(const MatchPattern *Pattern,
                           const MatchPatternInstantiation *Instantiation) {
  if (isa<DeclarationPattern>(Pattern))
    return true;
  if (const auto *Or = dyn_cast<OrPattern>(Pattern))
    return llvm::any_of(
        llvm::enumerate(Or->alternatives()), [&](auto Alternative) {
          return Instantiation->isViableOrAlternative(Or,
                                                      Alternative.index()) &&
                 hasPatternDeclarationsImpl(Alternative.value(), Instantiation);
        });
  if (const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern))
    return llvm::any_of(Instantiation->getDecompositionPatterns(Decomposition),
                        [&](const MatchPattern *Child) {
                          return hasPatternDeclarationsImpl(Child,
                                                            Instantiation);
                        });
  return llvm::any_of(Pattern->children(), [&](const MatchPattern *Child) {
    return hasPatternDeclarationsImpl(Child, Instantiation);
  });
}

static void
collectAndConjuncts(const MatchPattern *Pattern,
                    SmallVectorImpl<const MatchPattern *> &Conjuncts) {
  if (const auto *And = dyn_cast<AndPattern>(Pattern->IgnoreParens())) {
    for (const MatchPattern *Conjunct : And->conjuncts())
      collectAndConjuncts(Conjunct, Conjuncts);
    return;
  }
  Conjuncts.push_back(Pattern);
}

static bool
isFullyPlannablePattern(const MatchPattern *Pattern,
                        const MatchPatternInstantiation *Instantiation) {
  Pattern = Pattern->IgnoreParens();
  if (isa<WildcardPattern>(Pattern))
    return true;
  if (const auto *Or = dyn_cast<OrPattern>(Pattern))
    return llvm::all_of(
        llvm::enumerate(Or->alternatives()), [&](auto Alternative) {
          return !Instantiation->isViableOrAlternative(Or,
                                                       Alternative.index()) ||
                 isFullyPlannablePattern(Alternative.value(), Instantiation);
        });
  if (isa<AndPattern, DeclarationPattern>(Pattern))
    return false;
  if (const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern))
    return llvm::all_of(Instantiation->getDecompositionPatterns(Decomposition),
                        [&](const MatchPattern *Child) {
                          return isFullyPlannablePattern(Child, Instantiation);
                        });
  if (const auto *Alternative = dyn_cast<AlternativePattern>(Pattern)) {
    if (!getDecisionProjection(Alternative, Instantiation))
      return false;
    if (const TypePattern *Selector = Alternative->getTypeSelector()) {
      const MatchPatternInfo *SelectorInfo = Instantiation->find(Selector);
      if (SelectorInfo && SelectorInfo->Projection &&
          SelectorInfo->Projection->getKind() ==
              MatchProjection::CastProjection)
        return false;
    }
    return !Alternative->getSubPattern() ||
           isFullyPlannablePattern(Alternative->getSubPattern(), Instantiation);
  }
  if (isa<ExpressionPattern, TypePattern>(Pattern)) {
    if (getDecisionProjection(Pattern, Instantiation))
      return true;
    const MatchPatternInfo *Info = Instantiation->find(Pattern);
    if (!Info || !Info->Projection)
      return true;
    return isa<TypePattern>(Pattern)
               ? Info->Projection->getKind() == MatchProjection::CastProjection
               : Info->Projection->getKind() !=
                     MatchProjection::AlternativeProjection;
  }
  return false;
}

static SmallVector<MatchRow, 4>
expandPatternRows(MatchRow Row, const MatchPattern *Pattern,
                  const MatchPatternInstantiation *Instantiation,
                  ArrayRef<MatchPathComponent> SubjectPath,
                  const ASTContext &Context);

static SmallVector<MatchRow, 4>
expandDecompositionRows(MatchRow Row, ArrayRef<MatchPattern *> Patterns,
                        const MatchPatternInstantiation *Instantiation,
                        ArrayRef<MatchPathComponent> SubjectPath,
                        const MatchProjection *Projection,
                        const ASTContext &Context) {
  SmallVector<MatchRow, 4> Rows;
  Rows.push_back(std::move(Row));
  for (auto [Index, Pattern] : llvm::enumerate(Patterns)) {
    SmallVector<MatchPathComponent, 4> ChildPath(SubjectPath);
    ChildPath.push_back(
        {MatchPathComponent::Decomposition, Projection, unsigned(Index), {}});
    SmallVector<MatchRow, 4> Expanded;
    for (MatchRow &Current : Rows) {
      SmallVector<MatchRow, 4> PatternRows = expandPatternRows(
          std::move(Current), Pattern, Instantiation, ChildPath, Context);
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
                  ArrayRef<MatchPathComponent> SubjectPath,
                  const ASTContext &Context) {
  Pattern = Pattern->IgnoreParens();

  if (const auto *Or = dyn_cast<OrPattern>(Pattern)) {
    SmallVector<MatchRow, 4> Result;
    bool IsFullyPlannable = isFullyPlannablePattern(Or, Instantiation);
    if (!IsFullyPlannable)
      Row.AllowResidualTests = false;
    else
      Row.NeedsSharedAction = true;
    for (auto [I, Alternative] : llvm::enumerate(Or->alternatives())) {
      if (!Instantiation->isViableOrAlternative(Or, I))
        continue;
      SmallVector<MatchRow, 4> AlternativeRows = expandPatternRows(
          Row, Alternative, Instantiation, SubjectPath, Context);
      if (IsFullyPlannable)
        for (MatchRow &AlternativeRow : AlternativeRows)
          AlternativeRow.CompletedWhenExhausted.push_back(Or);
      Result.append(std::make_move_iterator(AlternativeRows.begin()),
                    std::make_move_iterator(AlternativeRows.end()));
    }
    return Result;
  }

  if (const auto *And = dyn_cast<AndPattern>(Pattern))
    return expandPatternRows(std::move(Row), And->conjuncts().front(),
                             Instantiation, SubjectPath, Context);

  if (isa<WildcardPattern>(Pattern)) {
    return singleRow(std::move(Row));
  }

  const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern);
  if (!Decomposition) {
    MatchColumn &Column = Row.Columns.emplace_back();
    Column.Pattern = Pattern;
    Column.SubjectPath.append(SubjectPath.begin(), SubjectPath.end());
    if (const MatchProjection *Projection =
            getDecisionProjection(Pattern, Instantiation)) {
      const MatchPatternInfo *Info = Instantiation->find(Pattern);
      Column.Projection = Projection;
      Column.MaterializeProjection = isa<DeclarationPattern>(Pattern);
      if (!Info->IsOpenAlternative &&
          Projection->getKind() == MatchProjection::AlternativeProjection) {
        Column.Kind = MatchColumn::ClosedAlternative;
        Column.AlternativeIndex = Info->SelectedAlternatives.front();
      } else {
        Column.Kind = MatchColumn::OpenProjection;
        Column.ProjectionType = Info->OpenAlternativeType;
        if (Column.ProjectionType.isNull() && Projection->getProjectedVar())
          Column.ProjectionType = Projection->getProjectedVar()->getType();
      }
    } else if (std::optional<IntegralPatternTest> Test =
                   getIntegralPatternTest(Pattern, Row, Context)) {
      Column.Kind = MatchColumn::IntegralValue;
      Column.IntegralComparison = Test->Comparison;
      Column.IntegralConstructor = std::move(Test->Value);
    }
    return singleRow(std::move(Row));
  }

  const MatchPatternInfo *Info = Instantiation->find(Decomposition);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  assert(Projection &&
         Projection->getKind() == MatchProjection::DecompositionProjection &&
         "expected decomposition projection");
  return expandDecompositionRows(
      std::move(Row), Instantiation->getDecompositionPatterns(Decomposition),
      Instantiation, SubjectPath, Projection, Context);
}

static SmallVector<MatchRow, 4> specializeColumn(const MatchRow &Source,
                                                 const MatchColumn *Column,
                                                 const ASTContext &Context) {
  unsigned Index = Column - Source.Columns.begin();
  MatchRow Row = Source;
  Row.KnownTests.push_back(Column->Pattern);
  Row.Columns.erase(Row.Columns.begin() + Index);

  const auto *Alternative = dyn_cast<AlternativePattern>(Column->Pattern);
  if (!Alternative || !Alternative->getSubPattern())
    return singleRow(std::move(Row));

  const MatchPatternInfo *Info =
      Source.Case->PatternInstantiation->find(Alternative);
  assert(Info && Info->Projection && "expected alternative projection");
  SmallVector<MatchPathComponent, 4> SubjectPath(Column->SubjectPath.begin(),
                                                 Column->SubjectPath.end());
  if (Column->Kind == MatchColumn::ClosedAlternative)
    SubjectPath.push_back({MatchPathComponent::ClosedAlternative,
                           Info->Projection,
                           Column->AlternativeIndex,
                           {}});
  else
    SubjectPath.push_back({MatchPathComponent::OpenAlternative,
                           Info->Projection, 0, Column->ProjectionType});
  return expandPatternRows(std::move(Row), Alternative->getSubPattern(),
                           Source.Case->PatternInstantiation, SubjectPath,
                           Context);
}

static bool canEmitResidualTest(const MatchColumn &Column,
                                const MatchRow &Row) {
  if (!Row.AllowResidualTests ||
      !isa<ExpressionPattern, TypePattern>(Column.Pattern))
    return false;
  const MatchPatternInfo *Info =
      Row.Case->PatternInstantiation->find(Column.Pattern);
  if (!Info || !Info->Projection)
    return true;
  return isa<TypePattern>(Column.Pattern)
             ? Info->Projection->getKind() == MatchProjection::CastProjection
             : Info->Projection->getKind() !=
                   MatchProjection::AlternativeProjection;
}

static MatchRow specializeResidualTest(const MatchRow &Source,
                                       const MatchColumn *Column) {
  unsigned Index = Column - Source.Columns.begin();
  MatchRow Row = Source;
  Row.CompletedPatterns.push_back(Column->Pattern);
  Row.Columns.erase(Row.Columns.begin() + Index);
  return Row;
}

static bool canNormalizeRow(const MatchRow &Row) {
  return Row.ActiveConjunct && Row.Columns.empty();
}

static SmallVector<MatchRow, 4> normalizeRow(MatchRow Row,
                                             const ASTContext &Context) {
  if (!canNormalizeRow(Row)) {
    return singleRow(std::move(Row));
  }

  for (const MatchPattern *Pattern : Row.CompletedWhenExhausted)
    if (!llvm::is_contained(Row.CompletedPatterns, Pattern))
      Row.CompletedPatterns.push_back(Pattern);
  Row.CompletedWhenExhausted.clear();
  if (!llvm::is_contained(Row.CompletedPatterns, Row.ActiveConjunct))
    Row.CompletedPatterns.push_back(Row.ActiveConjunct);
  Row.ActiveConjunct = nullptr;

  if (Row.RemainingConjuncts.empty()) {
    Row.CompletedPatterns.push_back(Row.SequencePattern);
    Row.SequencePattern = nullptr;
    return singleRow(std::move(Row));
  }

  const MatchPattern *Next = Row.RemainingConjuncts.front();
  Row.RemainingConjuncts.erase(Row.RemainingConjuncts.begin());
  Row.ActiveConjunct = Next;
  Row.AllowResidualTests = true;
  const MatchPatternInstantiation *Instantiation =
      Row.Case->PatternInstantiation;
  SmallVector<MatchRow, 4> Expanded =
      expandPatternRows(std::move(Row), Next, Instantiation, {}, Context);
  SmallVector<MatchRow, 4> Result;
  for (MatchRow &ExpandedRow : Expanded) {
    SmallVector<MatchRow, 4> Normalized =
        normalizeRow(std::move(ExpandedRow), Context);
    Result.append(std::make_move_iterator(Normalized.begin()),
                  std::make_move_iterator(Normalized.end()));
  }
  return Result;
}

static ArrayRef<MatchRow> dropCaseInstantiationRows(ArrayRef<MatchRow> Rows) {
  const MatchCaseInstantiation *Case = Rows.front().Case;
  auto FirstDifferent = llvm::find_if_not(
      Rows, [&](const MatchRow &Row) { return Row.Case == Case; });
  return Rows.drop_front(FirstDifferent - Rows.begin());
}

static ArrayRef<MatchRow> dropSourceCaseRows(ArrayRef<MatchRow> Rows) {
  unsigned CaseIndex = Rows.front().Case->CaseIndex;
  auto FirstDifferent = llvm::find_if_not(Rows, [&](const MatchRow &Row) {
    return Row.Case->CaseIndex == CaseIndex;
  });
  return Rows.drop_front(FirstDifferent - Rows.begin());
}

static std::unique_ptr<MatchDecisionTree::Node>
buildDecisionTree(ArrayRef<MatchRow> Rows, const ASTContext &Context) {
  using Node = MatchDecisionTree::Node;
  if (Rows.empty())
    return std::make_unique<Node>(Node::NK_Failure);

  if (llvm::any_of(Rows, canNormalizeRow)) {
    SmallVector<MatchRow, 8> NormalizedRows;
    for (const MatchRow &Row : Rows) {
      SmallVector<MatchRow, 4> Normalized = normalizeRow(Row, Context);
      NormalizedRows.append(std::make_move_iterator(Normalized.begin()),
                            std::make_move_iterator(Normalized.end()));
    }
    return buildDecisionTree(NormalizedRows, Context);
  }

  const MatchRow &First = Rows.front();
  auto BuildCandidate = [&] {
    auto Result = std::make_unique<Node>(Node::NK_Candidate);
    Result->MatchCandidate.Case = First.Case;
    Result->MatchCandidate.KnownTestPatterns = First.KnownTests;
    Result->MatchCandidate.CompletedPatterns = First.CompletedPatterns;
    bool Exhausted = First.Columns.empty();
    if (Exhausted)
      for (const MatchPattern *Pattern : First.CompletedWhenExhausted)
        if (!llvm::is_contained(Result->MatchCandidate.CompletedPatterns,
                                Pattern))
          Result->MatchCandidate.CompletedPatterns.push_back(Pattern);
    if (Exhausted && First.NeedsSharedAction &&
        !llvm::is_contained(Result->MatchCandidate.CompletedPatterns,
                            First.Case->Pattern))
      Result->MatchCandidate.CompletedPatterns.push_back(First.Case->Pattern);
    Result->MatchCandidate.ShareAction =
        First.NeedsSharedAction &&
        llvm::is_contained(Result->MatchCandidate.CompletedPatterns,
                           First.Case->Pattern);
    Result->Failure =
        buildDecisionTree(dropCaseInstantiationRows(Rows), Context);
    if (Rows.size() > 1 && Rows[1].Case->CaseIndex == First.Case->CaseIndex &&
        First.Case->Guard.hasGuard()) {
      Result->GuardFailure =
          buildDecisionTree(dropSourceCaseRows(Rows), Context);
    }
    return Result;
  };

  const MatchColumn *Selected = nullptr;
  unsigned SelectedFrequency = 0;
  // Among columns in the first row, choose the one tested by the greatest
  // number of remaining rows. This is Jacobs's duplication heuristic.
  for (const MatchColumn &Column : First.Columns) {
    if (Column.Kind == MatchColumn::Residual)
      continue;
    unsigned Frequency = llvm::count_if(Rows, [&](const MatchRow &Row) {
      return findTestColumn(Row, Column, Context) != nullptr;
    });
    if (Frequency > SelectedFrequency) {
      Selected = &Column;
      SelectedFrequency = Frequency;
    }
  }

  if (!Selected) {
    if (!First.Columns.empty() &&
        canEmitResidualTest(First.Columns.front(), First)) {
      const MatchColumn *Column = &First.Columns.front();
      SmallVector<MatchRow, 8> SuccessRows;
      SuccessRows.push_back(specializeResidualTest(First, Column));
      SuccessRows.append(Rows.drop_front().begin(), Rows.drop_front().end());

      auto Result = std::make_unique<Node>(Node::NK_PatternTest);
      Result->PatternMatch.Case = First.Case;
      Result->PatternMatch.Pattern = Column->Pattern;
      Result->PatternMatch.ProjectionPath =
          getProjectionPath(Column->SubjectPath);
      Result->Success = buildDecisionTree(SuccessRows, Context);
      Result->Failure = buildDecisionTree(Rows.drop_front(), Context);
      return Result;
    }
    return BuildCandidate();
  }

  SmallVector<MatchRow, 8> SuccessRows;
  SmallVector<MatchRow, 8> FailureRows;
  bool MaterializeProjection = false;
  for (const MatchRow &Row : Rows) {
    const MatchColumn *Column = findTestColumn(Row, *Selected, Context);
    // Rows without this column apply on both edges. A successful test consumes
    // the column; a different mutually exclusive constructor remains only on
    // the failure edge.
    if (!Column) {
      MatchRow Copied = Row;
      Copied.NeedsSharedAction = true;
      SuccessRows.push_back(Copied);
      FailureRows.push_back(std::move(Copied));
    } else if (hasSameConstructor(*Column, *Selected)) {
      MaterializeProjection |= Column->MaterializeProjection;
      SmallVector<MatchRow, 4> Specialized;
      if (Column->Kind == MatchColumn::IntegralValue)
        Specialized.push_back(specializeResidualTest(Row, Column));
      else
        Specialized = specializeColumn(Row, Column, Context);
      SuccessRows.append(std::make_move_iterator(Specialized.begin()),
                         std::make_move_iterator(Specialized.end()));
    } else {
      FailureRows.push_back(Row);
    }
  }

  auto Result = std::make_unique<Node>(
      Selected->Kind == MatchColumn::IntegralValue ? Node::NK_PatternTest
                                                   : Node::NK_Test);
  if (Selected->Kind == MatchColumn::IntegralValue) {
    Result->PatternMatch.Kind = MatchDecisionTree::PatternTest::IntegralValue;
    Result->PatternMatch.Case = First.Case;
    Result->PatternMatch.Pattern = Selected->Pattern;
    Result->PatternMatch.IntegralComparison = Selected->IntegralComparison;
    Result->PatternMatch.ProjectionPath =
        getProjectionPath(Selected->SubjectPath);
  } else {
    Result->MatchTest.Kind = Selected->Kind == MatchColumn::ClosedAlternative
                                 ? MatchDecisionTree::Test::ClosedAlternative
                                 : MatchDecisionTree::Test::OpenProjection;
    Result->MatchTest.Case = First.Case;
    if (Selected->Kind == MatchColumn::ClosedAlternative)
      Result->MatchTest.AlternativeIndex = Selected->AlternativeIndex;
    Result->MatchTest.Projection = Selected->Projection;
    Result->MatchTest.MaterializeProjection = MaterializeProjection;
    Result->MatchTest.ProjectionPath = getProjectionPath(Selected->SubjectPath);
  }
  Result->Success = buildDecisionTree(SuccessRows, Context);
  Result->Failure = buildDecisionTree(FailureRows, Context);
  return Result;
}

} // namespace

bool CodeGen::hasMatchPatternDeclarations(
    const MatchPattern *Pattern,
    const MatchPatternInstantiation *Instantiation) {
  return hasPatternDeclarationsImpl(Pattern, Instantiation);
}

MatchDecisionTree
MatchDecisionTree::build(ArrayRef<MatchCaseInstantiation> Cases,
                         const ASTContext &Context) {
  SmallVector<MatchRow, 8> Rows;
  Rows.reserve(Cases.size());
  for (const MatchCaseInstantiation &Case : Cases) {
    MatchRow Row;
    Row.Case = &Case;
    const MatchPattern *Pattern = Case.Pattern;
    SmallVector<const MatchPattern *, 4> Conjuncts;
    collectAndConjuncts(Pattern, Conjuncts);
    ArrayRef<const MatchPattern *> ConjunctList = Conjuncts;
    if (ConjunctList.size() > 1 &&
        llvm::none_of(ConjunctList.drop_back(),
                      [&](const MatchPattern *Conjunct) {
                        return hasMatchPatternDeclarations(
                            Conjunct, Case.PatternInstantiation);
                      })) {
      Row.SequencePattern = Case.Pattern;
      Row.ActiveConjunct = ConjunctList.front();
      Row.RemainingConjuncts.append(ConjunctList.drop_front().begin(),
                                    ConjunctList.drop_front().end());
      Pattern = Row.ActiveConjunct;
    }
    SmallVector<MatchRow, 4> Expanded = expandPatternRows(
        std::move(Row), Pattern, Case.PatternInstantiation, {}, Context);
    Rows.append(std::make_move_iterator(Expanded.begin()),
                std::make_move_iterator(Expanded.end()));
  }
  return MatchDecisionTree(buildDecisionTree(Rows, Context));
}
