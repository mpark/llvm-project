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
struct MatchRow {
  MatchCandidateRef Candidate;
  SmallVector<MatchOperation, 4> Columns;
  SmallVector<MatchBindingAction, 2> Bindings;
  bool ShareAction = false;
};

using Rows = SmallVector<MatchRow, 4>;

static Rows singleRow(MatchRow Row) {
  Rows Result;
  Result.push_back(std::move(Row));
  return Result;
}

static void appendRows(SmallVectorImpl<MatchRow> &Result, Rows Rows) {
  Result.append(std::make_move_iterator(Rows.begin()),
                std::make_move_iterator(Rows.end()));
}

static bool hasSameTestSubject(const MatchDecisionTest &LHS,
                               const MatchDecisionTest &RHS,
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
  case MatchDecisionTest::Residual:
  case MatchDecisionTest::Constant:
    return false;
  case MatchDecisionTest::ClosedAlternative:
    return true;
  case MatchDecisionTest::OpenProjection:
    if (LHS.Projection->getKind() != RHS.Projection->getKind())
      return false;
    if (!LHS.ProjectionType.isNull() || !RHS.ProjectionType.isNull())
      return !LHS.ProjectionType.isNull() && !RHS.ProjectionType.isNull() &&
             Context.hasSameType(LHS.ProjectionType, RHS.ProjectionType);
    return LHS.Projection == RHS.Projection;
  case MatchDecisionTest::IntegralValue:
    return Context.hasSameType(
        LHS.IntegralTest->Comparison->getLHS()->getType(),
        RHS.IntegralTest->Comparison->getLHS()->getType());
  }
  llvm_unreachable("unknown match test kind");
}

static bool isReady(const MatchDecisionTest &Test, const MatchRow &Row) {
  return llvm::none_of(Test.SubjectPath, [&](const MatchPathComponent &Part) {
    return llvm::any_of(Row.Columns, [&](const MatchOperation &Column) {
      return Column.Kind == MatchOperation::Test &&
             Column.MatchTest.Projection == Part.Projection;
    });
  });
}

static const MatchOperation *findTestColumn(const MatchRow &Row,
                                            const MatchDecisionTest &Test,
                                            const ASTContext &Context) {
  for (const MatchOperation &Column : Row.Columns) {
    if (Column.Kind == MatchOperation::Bindings)
      break;
    if (isReady(Column.MatchTest, Row) &&
        hasSameTestSubject(Column.MatchTest, Test, Context))
      return &Column;
  }
  return nullptr;
}

static bool hasSameConstructor(const MatchDecisionTest &LHS,
                               const MatchDecisionTest &RHS) {
  assert(LHS.Kind == RHS.Kind && "expected tests from the same family");
  switch (LHS.Kind) {
  case MatchDecisionTest::Residual:
  case MatchDecisionTest::Constant:
    llvm_unreachable("opaque tests are not constructors");
  case MatchDecisionTest::ClosedAlternative:
    return LHS.AlternativeIndex == RHS.AlternativeIndex;
  case MatchDecisionTest::OpenProjection:
    return true;
  case MatchDecisionTest::IntegralValue:
    return llvm::APSInt::isSameValue(LHS.IntegralTest->Value,
                                     RHS.IntegralTest->Value);
  }
  llvm_unreachable("unknown match test kind");
}

static const MatchIntegralPatternTest *
getIntegralPatternTest(const MatchPatternInfo &Info,
                       MatchCandidateRef Candidate) {
  if (!Info.IntegralTest ||
      Stmt::getLikelihood(Candidate.getAttributes()) != Stmt::LH_None ||
      Info.IntegralTest->Comparison->getLHS()
          ->IgnoreParenImpCasts()
          ->getType()
          .isVolatileQualified())
    return nullptr;
  return Info.IntegralTest;
}

static void addProjection(MatchRow &Row, const MatchProjection *Projection,
                          ArrayRef<MatchPathComponent> Path) {
  MatchBindingAction &Action = Row.Bindings.emplace_back();
  Action.Kind = MatchBindingAction::Projection;
  Action.Project = Projection;
  Action.SubjectPath.append(Path.begin(), Path.end());
}

static Rows expandPatternRows(MatchRow Row, const MatchPattern *Pattern,
                              ArrayRef<MatchPathComponent> Path) {
  Pattern = Pattern->IgnoreParens();
  const MatchPatternInstantiation *Instantiation =
      Row.Candidate.getPatternInstantiation();

  if (const auto *Or = dyn_cast<OrPattern>(Pattern)) {
    Rows Result;
    Row.ShareAction = true;
    for (auto [I, Alternative] : llvm::enumerate(Or->alternatives()))
      if (Instantiation->isViableOrAlternative(Or, I))
        appendRows(Result, expandPatternRows(Row, Alternative, Path));
    return Result;
  }

  if (const auto *And = dyn_cast<AndPattern>(Pattern)) {
    Rows Result = singleRow(std::move(Row));
    for (auto [I, Conjunct] : llvm::enumerate(And->conjuncts())) {
      Rows Expanded;
      for (MatchRow &Current : Result) {
        unsigned BindingBegin = Current.Bindings.size();
        Rows ConjunctRows =
            expandPatternRows(std::move(Current), Conjunct, Path);
        if (I + 1 != And->conjuncts().size())
          for (MatchRow &ConjunctRow : ConjunctRows) {
            MatchOperation &Barrier = ConjunctRow.Columns.emplace_back();
            Barrier.Kind = MatchOperation::Bindings;
            Barrier.BindingActions.append(ConjunctRow.Bindings.begin() +
                                              BindingBegin,
                                          ConjunctRow.Bindings.end());
            ConjunctRow.Bindings.resize(BindingBegin);
          }
        appendRows(Expanded, std::move(ConjunctRows));
      }
      Result = std::move(Expanded);
    }
    return Result;
  }

  if (isa<WildcardPattern>(Pattern))
    return singleRow(std::move(Row));

  const MatchPatternInfo *Info = Instantiation->find(Pattern);
  const MatchProjection *Projection = Info ? Info->Projection : nullptr;
  if (const auto *Decomposition = dyn_cast<DecompositionPattern>(Pattern)) {
    assert(Projection && Projection->getDecomposedDecl() &&
           "expected decomposition projection");
    addProjection(Row, Projection, Path);
    Rows Result = singleRow(std::move(Row));
    for (auto [I, Child] : llvm::enumerate(
             Instantiation->getDecompositionPatterns(Decomposition))) {
      SmallVector<MatchPathComponent, 4> ChildPath(Path);
      ChildPath.push_back(
          {MatchPathComponent::Decomposition, Projection, unsigned(I), {}});
      Rows Expanded;
      for (MatchRow &Current : Result)
        appendRows(Expanded,
                   expandPatternRows(std::move(Current), Child, ChildPath));
      Result = std::move(Expanded);
    }
    return Result;
  }

  if (const auto *Declaration = dyn_cast<DeclarationPattern>(Pattern)) {
    MatchBindingAction &Action = Row.Bindings.emplace_back();
    Action.Kind = MatchBindingAction::Declaration;
    Action.Declare = Declaration;
    Action.SubjectPath.append(Path.begin(), Path.end());
    if (!Projection || Projection->getKind() != MatchProjection::CastProjection)
      return singleRow(std::move(Row));
  } else if (isa<TypePattern>(Pattern) &&
             (!Projection ||
              Projection->getKind() != MatchProjection::CastProjection)) {
    assert(Info && "expected checked type pattern");
    if (Info->TypePatternResolved)
      return singleRow(std::move(Row));
  }

  MatchDecisionTest &Test = Row.Columns.emplace_back().MatchTest;
  Test.Candidate = Row.Candidate;
  Test.Condition = Info && Info->Condition ? Info->Condition
                   : Projection            ? Projection->getConditionExpr()
                                           : nullptr;
  Test.Projection = Projection;
  Test.SubjectPath.append(Path.begin(), Path.end());
  if (Projection && Projection->getIntermediateVar() &&
      (Projection->getKind() == MatchProjection::CastProjection ||
       Info->IsOpenAlternative || Info->SelectedAlternatives.size() == 1)) {
    if (Projection->getKind() == MatchProjection::AlternativeProjection &&
        !Info->IsOpenAlternative) {
      Test.Kind = MatchDecisionTest::ClosedAlternative;
      Test.AlternativeIndex = Info->SelectedAlternatives.front();
    } else {
      Test.Kind = MatchDecisionTest::OpenProjection;
      Test.ProjectionType = Info->OpenAlternativeType;
      if (Test.ProjectionType.isNull() && Projection->getProjectedVar())
        Test.ProjectionType = Projection->getProjectedVar()->getType();
    }
  } else if (isa<ExpressionPattern>(Pattern)) {
    if (const MatchIntegralPatternTest *Integral =
            getIntegralPatternTest(*Info, Row.Candidate)) {
      Test.Kind = MatchDecisionTest::IntegralValue;
      Test.IntegralTest = Integral;
    }
  } else if (isa<TypePattern>(Pattern)) {
    Test.Kind = MatchDecisionTest::Constant;
    Test.ConstantValue = Info->TypePatternResolved;
  }

  if (const auto *Alternative = dyn_cast<AlternativePattern>(Pattern)) {
    assert(Projection && "expected alternative projection");
    unsigned Index = Test.AlternativeIndex;
    QualType Type = Test.ProjectionType;
    addProjection(Row, Projection, Path);
    const MatchPattern *SubPattern = Alternative->getSubPattern();
    if (!SubPattern)
      return singleRow(std::move(Row));
    SmallVector<MatchPathComponent, 4> ChildPath(Path);
    ChildPath.push_back({Info->IsOpenAlternative
                             ? MatchPathComponent::OpenAlternative
                             : MatchPathComponent::ClosedAlternative,
                         Projection, Index, Type});
    if (const TypePattern *Selector = Alternative->getTypeSelector()) {
      const MatchPatternInfo *SelectorInfo = Instantiation->find(Selector);
      const MatchProjection *Cast =
          SelectorInfo ? SelectorInfo->Projection : nullptr;
      if (Cast && Cast->getKind() == MatchProjection::CastProjection) {
        Rows Selected = expandPatternRows(std::move(Row), Selector, ChildPath);
        assert(Selected.size() == 1 && "cast selector expanded into rows");
        Row = std::move(Selected.front());
        ChildPath.push_back({MatchPathComponent::Cast, Cast, 0,
                             Cast->getProjectedVar()->getType()});
      }
    }
    return expandPatternRows(std::move(Row), SubPattern, ChildPath);
  }
  return singleRow(std::move(Row));
}

static MatchRow consumeTest(const MatchRow &Source,
                            const MatchOperation *Column) {
  MatchRow Row = Source;
  Row.Columns.erase(Row.Columns.begin() + (Column - Source.Columns.begin()));
  return Row;
}

static bool hasLeadingBarrier(const MatchRow &Row) {
  return !Row.Columns.empty() &&
         Row.Columns.front().Kind == MatchOperation::Bindings &&
         Row.Columns.front().BindingActions.empty();
}

static ArrayRef<MatchRow> dropSourceCaseRows(ArrayRef<MatchRow> Rows) {
  unsigned SourceIndex = Rows.front().Candidate.getSourceIndex();
  auto FirstDifferent = llvm::find_if_not(Rows, [&](const MatchRow &Row) {
    return Row.Candidate.getSourceIndex() == SourceIndex;
  });
  return Rows.drop_front(FirstDifferent - Rows.begin());
}

static std::unique_ptr<MatchDecisionTree::Node>
buildDecisionTree(ArrayRef<MatchRow> Rows, const ASTContext &Context) {
  using Node = MatchDecisionTree::Node;
  if (Rows.empty())
    return std::make_unique<Node>(Node::NK_Failure);

  if (llvm::any_of(Rows, hasLeadingBarrier)) {
    SmallVector<MatchRow, 8> Normalized(Rows);
    for (MatchRow &Row : Normalized)
      while (hasLeadingBarrier(Row))
        Row.Columns.erase(Row.Columns.begin());
    return buildDecisionTree(Normalized, Context);
  }

  const MatchRow &First = Rows.front();
  const MatchOperation *Selected = nullptr;
  unsigned SelectedFrequency = 0;
  // Peterse's heuristic chooses among ready columns of the first row. A
  // payload is ready only after its enclosing constructor tests have passed.
  for (const MatchOperation &Column : First.Columns) {
    if (Column.Kind == MatchOperation::Bindings)
      break;
    if (!isReady(Column.MatchTest, First) ||
        Column.MatchTest.Kind == MatchDecisionTest::Residual ||
        Column.MatchTest.Kind == MatchDecisionTest::Constant)
      continue;
    unsigned Frequency = llvm::count_if(Rows, [&](const MatchRow &Row) {
      return findTestColumn(Row, Column.MatchTest, Context) != nullptr;
    });
    if (Frequency > SelectedFrequency) {
      Selected = &Column;
      SelectedFrequency = Frequency;
    }
  }

  if (!Selected && (First.Columns.empty() ||
                    First.Columns.front().Kind == MatchOperation::Bindings)) {
    auto Result = std::make_unique<Node>(Node::NK_Candidate);
    Result->MatchCandidate = {First.Candidate, First.Columns, First.Bindings,
                              First.ShareAction};
    Result->Failure = buildDecisionTree(Rows.drop_front(), Context);
    if (First.Candidate.getGuard().hasGuard() && Rows.size() > 1 &&
        Rows[1].Candidate.getSourceIndex() == First.Candidate.getSourceIndex())
      Result->GuardFailure =
          buildDecisionTree(dropSourceCaseRows(Rows), Context);
    return Result;
  }

  auto Result = std::make_unique<Node>(Node::NK_Test);
  if (!Selected) {
    const MatchOperation *Column = &First.Columns.front();
    assert(isReady(Column->MatchTest, First) && "unresolved test dependency");
    Result->MatchTest = Column->MatchTest;
    SmallVector<MatchRow, 8> SuccessRows;
    SuccessRows.push_back(consumeTest(First, Column));
    SuccessRows.append(Rows.drop_front().begin(), Rows.drop_front().end());
    Result->Success = buildDecisionTree(SuccessRows, Context);
    Result->Failure = buildDecisionTree(Rows.drop_front(), Context);
    return Result;
  }

  SmallVector<MatchRow, 8> SuccessRows;
  SmallVector<MatchRow, 8> FailureRows;
  for (const MatchRow &Row : Rows) {
    const MatchOperation *Column =
        findTestColumn(Row, Selected->MatchTest, Context);
    if (!Column) {
      MatchRow Copied = Row;
      Copied.ShareAction = true;
      SuccessRows.push_back(Copied);
      FailureRows.push_back(std::move(Copied));
    } else if (hasSameConstructor(Column->MatchTest, Selected->MatchTest)) {
      SuccessRows.push_back(consumeTest(Row, Column));
    } else {
      FailureRows.push_back(Row);
    }
  }
  Result->MatchTest = Selected->MatchTest;
  Result->Success = buildDecisionTree(SuccessRows, Context);
  Result->Failure = buildDecisionTree(FailureRows, Context);
  return Result;
}

static const CXXDynamicCastExpr *
getBatchableMatchCast(const MatchDecisionTree::Node &Node) {
  if (Node.Kind != MatchDecisionTree::Node::NK_Test ||
      Node.MatchTest.Kind != MatchDecisionTest::OpenProjection ||
      !Node.MatchTest.SubjectPath.empty() ||
      Node.MatchTest.Projection->getKind() != MatchProjection::CastProjection ||
      Node.MatchTest.Candidate.getGuard().hasGuard() ||
      Node.Success->Kind != MatchDecisionTree::Node::NK_Candidate ||
      Node.Success->MatchCandidate.Record != Node.MatchTest.Candidate ||
      !Node.Success->MatchCandidate.Operations.empty())
    return nullptr;

  const MatchPattern *Pattern =
      Node.MatchTest.Candidate.getPattern()->IgnoreParens();
  QualType PatternType;
  if (const auto *Declaration = dyn_cast<DeclarationPattern>(Pattern))
    PatternType = Declaration->getDeclaration()->getType();
  else if (const auto *Type = dyn_cast<TypePattern>(Pattern))
    PatternType = Type->getType();
  else
    return nullptr;
  if (!PatternType->isReferenceType())
    return nullptr;

  const auto *Cast = dyn_cast<CXXDynamicCastExpr>(
      Node.MatchTest.Projection->getIntermediateVar()
          ->getInit()
          ->IgnoreParenImpCasts());
  if (!Cast || Cast->getCastKind() != CK_Dynamic || Cast->isAlwaysNull())
    return nullptr;
  QualType Source = Cast->getSubExpr()->getType()->getPointeeType();
  QualType Target = Cast->getType()->getPointeeType();
  if (Source.isVolatileQualified() || Target.isNull() ||
      !Target->isRecordType() || Source.getAddressSpace() != LangAS::Default ||
      Target.getAddressSpace() != LangAS::Default ||
      Target->getAsCXXRecordDecl()->isEffectivelyFinal())
    return nullptr;
  return Cast;
}

/// Group tests after binary specialization, independently of column choice.
static std::unique_ptr<MatchDecisionTree::Node>
groupDecisionTests(std::unique_ptr<MatchDecisionTree::Node> Root,
                   const ASTContext &Context, bool EnableBatchedDynamicCast) {
  using Node = MatchDecisionTree::Node;
  if (EnableBatchedDynamicCast) {
    if (const CXXDynamicCastExpr *First = getBatchableMatchCast(*Root)) {
      const VarDecl *Holding = Root->MatchTest.Projection->getHoldingVar();
      const Expr *Subject = Holding->getInit()->IgnoreParenImpCasts();
      unsigned Count = 0;
      const Node *Default = Root.get();
      while (const CXXDynamicCastExpr *Cast = getBatchableMatchCast(*Default)) {
        if (Default->MatchTest.Projection->getHoldingVar()
                    ->getInit()
                    ->IgnoreParenImpCasts() != Subject ||
            !Context.hasSameType(Cast->getSubExpr()->getType(),
                                 First->getSubExpr()->getType()))
          break;
        ++Count;
        Default = Default->Failure.get();
      }
      if (Count > 1) {
        auto Result = std::make_unique<Node>(Node::NK_BatchedCast);
        Result->MatchBatchedCast.SourceCast = First;
        Result->MatchBatchedCast.HoldingVar = Holding;
        for (unsigned I = 0; I != Count; ++I) {
          const CXXDynamicCastExpr *Cast = getBatchableMatchCast(*Root);
          Result->MatchBatchedCast.Targets.push_back(
              {Cast->getType()->getPointeeType().getUnqualifiedType(),
               std::move(Root->Success)});
          std::unique_ptr<Node> Next = std::move(Root->Failure);
          Root = std::move(Next);
        }
        Result->Failure = groupDecisionTests(std::move(Root), Context,
                                             EnableBatchedDynamicCast);
        return Result;
      }
    }
  }

  auto GetValue = [&](const Node &Node) -> std::optional<llvm::APSInt> {
    if (Root->Kind != MatchDecisionTree::Node::NK_Test ||
        Node.Kind != MatchDecisionTree::Node::NK_Test ||
        Root->MatchTest.Kind != Node.MatchTest.Kind)
      return std::nullopt;
    const MatchDecisionTest &First = Root->MatchTest;
    const MatchDecisionTest &Test = Node.MatchTest;
    if (First.Kind == MatchDecisionTest::IntegralValue) {
      if (First.IntegralTest->Comparison->getLHS()->IgnoreParenImpCasts() !=
              Test.IntegralTest->Comparison->getLHS()->IgnoreParenImpCasts() ||
          !Context.hasSameType(
              First.IntegralTest->Comparison->getLHS()->getType(),
              Test.IntegralTest->Comparison->getLHS()->getType()) ||
          First.SubjectPath.size() != Test.SubjectPath.size() ||
          !llvm::all_of(llvm::zip_equal(First.SubjectPath, Test.SubjectPath),
                        [](auto Parts) {
                          return std::get<0>(Parts).Projection ==
                                 std::get<1>(Parts).Projection;
                        }))
        return std::nullopt;
      return Test.IntegralTest->Value;
    }
    if (First.Kind == MatchDecisionTest::ClosedAlternative) {
      const VarDecl *Index = First.Projection->getIntermediateVar();
      if (!Index->getType()->isIntegralOrEnumerationType() ||
          Test.Projection->getIntermediateVar() != Index)
        return std::nullopt;
      return llvm::APSInt(
          llvm::APInt(Context.getIntWidth(Index->getType()),
                      Test.AlternativeIndex),
          Index->getType()->isUnsignedIntegerOrEnumerationType());
    }
    return std::nullopt;
  };

  SmallVector<llvm::APSInt, 4> Values;
  const Node *Default = Root.get();
  while (std::optional<llvm::APSInt> Value = GetValue(*Default)) {
    if (llvm::is_contained(Values, *Value))
      break;
    Values.push_back(std::move(*Value));
    Default = Default->Failure.get();
  }
  if (!Values.empty() &&
      (Root->MatchTest.Kind == MatchDecisionTest::ClosedAlternative ||
       Values.size() > 1)) {
    auto Result = std::make_unique<Node>(Node::NK_Switch);
    Result->MatchTest = Root->MatchTest;
    for (llvm::APSInt &Value : Values) {
      Result->MatchSwitch.Branches.push_back(
          {std::move(Value),
           groupDecisionTests(std::move(Root->Success), Context,
                              EnableBatchedDynamicCast)});
      std::unique_ptr<Node> Next = std::move(Root->Failure);
      Root = std::move(Next);
    }
    Result->Failure =
        groupDecisionTests(std::move(Root), Context, EnableBatchedDynamicCast);
    return Result;
  }

  if (Root->Success)
    Root->Success = groupDecisionTests(std::move(Root->Success), Context,
                                       EnableBatchedDynamicCast);
  if (Root->Failure)
    Root->Failure = groupDecisionTests(std::move(Root->Failure), Context,
                                       EnableBatchedDynamicCast);
  if (Root->GuardFailure)
    Root->GuardFailure = groupDecisionTests(std::move(Root->GuardFailure),
                                            Context, EnableBatchedDynamicCast);
  return Root;
}

template <class CheckedCandidate>
static std::unique_ptr<MatchDecisionTree::Node>
buildMatchDecision(ArrayRef<CheckedCandidate> Candidates,
                   const ASTContext &Context, bool EnableBatchedDynamicCast) {
  SmallVector<MatchRow, 8> Rows;
  for (const CheckedCandidate &Candidate : Candidates) {
    MatchRow Row;
    Row.Candidate = MatchCandidateRef(Candidate);
    appendRows(Rows, expandPatternRows(std::move(Row), Candidate.Pattern, {}));
  }
  return groupDecisionTests(buildDecisionTree(Rows, Context), Context,
                            EnableBatchedDynamicCast);
}
} // namespace

bool CodeGen::hasMatchPatternDeclarations(
    const MatchPattern *Pattern,
    const MatchPatternInstantiation *Instantiation) {
  return anyMatchPattern(Pattern, Instantiation, [](const MatchPattern *P) {
    return isa<DeclarationPattern>(P);
  });
}

MatchDecisionTree
MatchDecisionTree::build(ArrayRef<MatchCaseInstantiation> Cases,
                         const ASTContext &Context,
                         bool EnableBatchedDynamicCast) {
  return MatchDecisionTree(
      buildMatchDecision(Cases, Context, EnableBatchedDynamicCast));
}

MatchDecisionTree
MatchDecisionTree::build(ArrayRef<MatchTestInstantiation> Tests,
                         const ASTContext &Context,
                         bool EnableBatchedDynamicCast) {
  return MatchDecisionTree(
      buildMatchDecision(Tests, Context, EnableBatchedDynamicCast));
}
