//===- unittests/AST/MatchTest.cpp - Match AST tests
//-------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTNodeTraverser.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace clang;

namespace {

constexpr llvm::StringLiteral Traits = R"cpp(
namespace std {
template <class T> struct alternative_traits;
struct alternative_info {
  decltype(^^int) info;
  bool empty;
  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
}

template <class A, class B> struct Choice {
  unsigned active;
  A first;
  B second;
};
template <class A, class B> struct std::alternative_traits<Choice<A, B>> {
  static constexpr alternative_info alternatives[] = {^^A, ^^B};
  static constexpr bool has_residual_states = false;
  static unsigned index(const Choice<A, B> &value) noexcept {
    return value.active;
  }
  template <unsigned I, class Self> static decltype(auto) get(Self &&value) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(value).first);
    else
      return (static_cast<Self &&>(value).second);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
};
template <> struct std::alternative_traits<MaybeInt> {
  static constexpr alternative_info alternatives[] = {{{}, true}, ^^int};
  static constexpr bool has_residual_states = false;
  static unsigned index(const MaybeInt &value) noexcept { return value.engaged; }
  template <unsigned I, class Self> static decltype(auto) get(Self &&value) {
    static_assert(I == 1);
    return (static_cast<Self &&>(value).value);
  }
};

bool keep(int);
bool keep(double);
void use(int);
void use(double);
)cpp";

std::unique_ptr<ASTUnit> buildMatchAST(llvm::StringRef Code) {
  auto AST = tooling::buildASTFromCodeWithArgs(
      (Traits + Code).str(),
      {"-std=c++2d", "-fpattern-matching", "-Wno-redundant-match-cases"});
  if (AST)
    EXPECT_FALSE(AST->getDiagnostics().hasErrorOccurred());
  return AST;
}

FunctionDecl *getFunction(ASTUnit &AST, llvm::StringRef Name) {
  for (Decl *D : AST.getASTContext().getTranslationUnitDecl()->decls()) {
    if (auto *Template = dyn_cast<FunctionTemplateDecl>(D))
      D = Template->getTemplatedDecl();
    if (auto *Function = dyn_cast<FunctionDecl>(D);
        Function && Function->getName() == Name)
      return Function;
  }
  return nullptr;
}

class MatchFinder : public RecursiveASTVisitor<MatchFinder> {
public:
  MatchSelectExpr *Selection = nullptr;
  MatchTestExpr *Test = nullptr;

  bool VisitMatchSelectExpr(MatchSelectExpr *E) {
    if (!Selection)
      Selection = E;
    return true;
  }
  bool VisitMatchTestExpr(MatchTestExpr *E) {
    if (!Test)
      Test = E;
    return true;
  }
};

struct VisitCounts {
  unsigned Payloads = 0;
  unsigned DependentPayloads = 0;
  unsigned Markers = 0;
  unsigned Guards = 0;
  unsigned Regions = 0;

  void record(const Decl *D) {
    if (isa_and_nonnull<MatchTemplateRegionDecl>(D))
      ++Regions;
    if (const auto *Variable = dyn_cast_or_null<VarDecl>(D)) {
      if (Variable->getName() == "payload") {
        ++Payloads;
        DependentPayloads += Variable->getType()->isDependentType();
      } else if (Variable->getName() == "marker") {
        ++Markers;
      } else if (Variable->getName() == "guard") {
        ++Guards;
      }
    }
  }
};

class MatchVisitor : public RecursiveASTVisitor<MatchVisitor> {
  bool VisitInstantiations;

public:
  VisitCounts Counts;
  explicit MatchVisitor(bool VisitInstantiations)
      : VisitInstantiations(VisitInstantiations) {}
  bool shouldVisitTemplateInstantiations() const { return VisitInstantiations; }
  bool shouldVisitImplicitCode() const { return true; }
  bool VisitDecl(Decl *D) {
    Counts.record(D);
    return true;
  }
};

struct NodeRecorder {
  VisitCounts Counts;
  template <class F> void AddChild(F Fn) { Fn(); }
  template <class F> void AddChild(llvm::StringRef, F Fn) { Fn(); }
  void Visit(const Decl *D) { Counts.record(D); }
  template <class... T> void Visit(T...) {}
};

class MatchTraverser : public ASTNodeTraverser<MatchTraverser, NodeRecorder> {
public:
  NodeRecorder Recorder;
  NodeRecorder &doGetNodeDelegate() { return Recorder; }
};

TEST(MatchAST, InstantiationsBySourceCase) {
  auto AST = buildMatchAST(R"cpp(
int test(Choice<int, double> value) {
  return match (value) {
    case { int payload } if (keep(payload)) => payload;
    case { auto payload } => static_cast<int>(payload);
    case _ => 0;
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  MatchSelectExpr *Selection = Finder.Selection;
  ASSERT_NE(Selection, nullptr);
  ASSERT_EQ(Selection->getNumCases(), 3u);
  ASSERT_EQ(Selection->getNumCaseInstantiations(), 3u);
  auto All = Selection->getCaseInstantiations();
  auto First = Selection->getCases()[0].getInstantiations();
  auto Second = Selection->getCases()[1].getInstantiations();
  EXPECT_EQ(First.size(), 1u);
  EXPECT_EQ(First.data(), All.data());
  EXPECT_EQ(Second.size(), 2u);
  EXPECT_EQ(Second.data(), All.data() + 1);
  EXPECT_TRUE(Selection->getCases()[2].getInstantiations().empty());
  for (auto [CaseIndex, Case] : llvm::enumerate(Selection->getCases())) {
    MatchCaseInstantiation Direct = Case.getDirectInstantiation(CaseIndex);
    EXPECT_EQ(Direct.CaseIndex, CaseIndex);
    EXPECT_EQ(Direct.PatternInstantiation, Case.PatternInstantiation);
    EXPECT_TRUE(Direct.sharesSyntaxWith(Case));
    for (const MatchCaseInstantiation &Instantiation : Case.getInstantiations())
      EXPECT_EQ(Instantiation.CaseIndex, CaseIndex);
  }
}

TEST(MatchAST, InstantiationsSkipCoveredSourceCases) {
  auto AST = buildMatchAST(R"cpp(
int test(Choice<int, double> value) {
  return match (value) {
    case { int payload } => payload;
    case { int payload } => payload;
    case { double payload } if (keep(payload)) => static_cast<int>(payload);
    case { auto payload } => static_cast<int>(payload);
    case _ => 0;
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  MatchSelectExpr *Selection = Finder.Selection;
  ASSERT_NE(Selection, nullptr);
  ASSERT_EQ(Selection->getNumCases(), 5u);
  ASSERT_EQ(Selection->getNumCaseInstantiations(), 3u);
  auto All = Selection->getCaseInstantiations();
  auto Cases = Selection->getCases();
  EXPECT_EQ(Cases[0].getInstantiations().size(), 1u);
  EXPECT_EQ(Cases[0].getInstantiations().data(), All.data());
  EXPECT_TRUE(Cases[1].getInstantiations().empty());
  EXPECT_EQ(Cases[2].getInstantiations().size(), 1u);
  EXPECT_EQ(Cases[2].getInstantiations().data(), All.data() + 1);
  EXPECT_EQ(Cases[3].getInstantiations().size(), 1u);
  EXPECT_EQ(Cases[3].getInstantiations().data(), All.data() + 2);
  EXPECT_TRUE(Cases[4].getInstantiations().empty());
}

TEST(MatchAST, InstantiationsSkipDiscardedConstexprCases) {
  auto AST = buildMatchAST(R"cpp(
int test() {
  return match constexpr (1) {
    case 0 => 0;
    case 1 => 1;
    case _ => 2;
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  MatchSelectExpr *Selection = Finder.Selection;
  ASSERT_NE(Selection, nullptr);
  ASSERT_EQ(Selection->getNumCases(), 3u);
  ASSERT_EQ(Selection->getNumCaseInstantiations(), 1u);
  auto Cases = Selection->getCases();
  EXPECT_TRUE(Cases[0].getInstantiations().empty());
  EXPECT_EQ(Cases[1].getInstantiations().size(), 1u);
  EXPECT_EQ(Cases[1].getInstantiations().data(),
            Selection->getCaseInstantiations().data());
  EXPECT_TRUE(Cases[2].getInstantiations().empty());
}

TEST(MatchAST, DirectPlaceholderInstantiationsRespectCoverage) {
  auto AST = buildMatchAST(R"cpp(
void covered(int value) {
  match (value) {
    case int => ;
    case auto payload => payload.size();
  }
}
void guarded(int value) {
  match (value) {
    case int if (value == 0) => ;
    case auto payload => use(payload);
  }
}
struct Product {
  int first;
  int second;
};
void decomposed(Product value) {
  match (value) {
    case [int first, int second] => ;
    case [auto first, auto second] => first.size();
  }
}
void structured(Product value) {
  match (value) {
    case [int first, int second] => ;
    case auto [first, second] => first.size();
  }
}
)cpp");
  ASSERT_TRUE(AST);
  const struct {
    const char *Name;
    unsigned InstantiationCount;
  } Cases[] = {
      {"covered", 0}, {"guarded", 1}, {"decomposed", 0}, {"structured", 0}};
  for (auto [Name, InstantiationCount] : Cases) {
    SCOPED_TRACE(Name);
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    MatchSelectExpr *Selection = Finder.Selection;
    ASSERT_NE(Selection, nullptr);
    ASSERT_EQ(Selection->getNumCases(), 2u);
    EXPECT_EQ(Selection->getCases()[0].Region, nullptr);
    EXPECT_NE(Selection->getCases()[1].Region, nullptr);
    EXPECT_EQ(Selection->getCases()[1].getInstantiations().size(),
              InstantiationCount);
  }
}

TEST(MatchAST, CaseRangesAreReboundWhenRebuilding) {
  auto AST = buildMatchAST(R"cpp(
int test(Choice<int, double> value) {
  return match (value) {
    case { auto payload } => static_cast<int>(payload);
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  MatchSelectExpr *Selection = Finder.Selection;
  ASSERT_NE(Selection, nullptr);
  auto Rebuild = [&](ArrayRef<MatchCaseInstantiation> Instantiations) {
    return MatchSelectExpr::Create(
        AST->getASTContext(), Selection->getInitStmt(),
        Selection->getHoldingVar(), Selection->getSubject(),
        Selection->getMatchLoc(), Selection->isConstexpr(),
        Selection->isStatement(), Selection->hasSubjectProduct(),
        Selection->isFullyCovered(), Selection->getOrigResultType(),
        Selection->getType(), Selection->getPreamble(), Selection->getCases(),
        Instantiations, Selection->getBraces());
  };
  auto Original = Selection->getCases()[0].getInstantiations();
  ASSERT_EQ(Original.size(), 2u);
  MatchSelectExpr *Copy = Rebuild(Selection->getCaseInstantiations());
  auto Copied = Copy->getCases()[0].getInstantiations();
  EXPECT_EQ(Copied.size(), Original.size());
  EXPECT_EQ(Copied.data(), Copy->getCaseInstantiations().data());
  EXPECT_NE(Copied.data(), Original.data());
  EXPECT_EQ(Selection->getCases()[0].getInstantiations().data(),
            Original.data());

  MatchSelectExpr *Uninstantiated = Rebuild({});
  EXPECT_TRUE(Uninstantiated->getCases()[0].getInstantiations().empty());
  EXPECT_EQ(Selection->getCases()[0].getInstantiations().data(),
            Original.data());
}

TEST(MatchAST, IrrefutabilityUsesCompletePattern) {
  auto AST = buildMatchAST(R"cpp(
struct Product {
  MaybeInt optional;
  int integer;
};

bool wildcard(int value) { return match(value, case _); }
bool product(Product value) { return match(value, case [_, _]); }
bool empty(MaybeInt value) { return match(value, case {}); }
bool payload(MaybeInt value) { return match(value, case { int payload }); }
bool closed_or_wildcard(MaybeInt value) {
  return match(value, case {} or _);
}
bool nested_closed(Product value) { return match(value, case [{}, _]); }
bool nested_or_wildcard(Product value) {
  return match(value, case [{} or _, _]);
}
)cpp");
  ASSERT_TRUE(AST);
  const struct {
    const char *Name;
    bool Irrefutable;
  } Cases[] = {{"wildcard", true},
               {"product", true},
               {"empty", false},
               {"payload", false},
               {"closed_or_wildcard", true},
               {"nested_closed", false},
               {"nested_or_wildcard", true}};
  for (auto [Name, Irrefutable] : Cases) {
    SCOPED_TRACE(Name);
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Test, nullptr);
    EXPECT_EQ(Finder.Test->isPatternIrrefutable(), Irrefutable);
  }
}

TEST(MatchAST, TemplateRegionsFollowPlaceholders) {
  auto AST = buildMatchAST(R"cpp(
void singleton(MaybeInt value) {
  match (value) {
    case { auto payload } => use(payload);
    case {} => ;
  }
}
void multiple(Choice<int, double> value) {
  match (value) {
    case { auto payload } => use(payload);
  }
}
void repeated(Choice<int, int> value) {
  match (value) {
    case { auto payload } => use(payload);
  }
}
void selected(MaybeInt value) {
  match (value) {
    case { int: auto payload } => use(payload);
    case {} => ;
  }
}
void omitted(MaybeInt value) {
  match (value) {
    case { auto } => ;
    case {} => ;
  }
}
template <int N>
void already_dependent(MaybeInt value) {
  match (value) {
    case { auto payload } => use(payload);
    case {} => ;
  }
}
template <class T>
void dependent_whole(T value) {
  match (value) {
    case auto whole => (void)whole;
  }
}
void value_or(MaybeInt value) {
  match (value) {
    case { 42 } or { 43 } => ;
    case _ => ;
  }
}
void bound_or(Choice<int, double> value) {
  match (value) {
    case { int payload } or { double payload } => use(payload);
  }
}
void concrete(Choice<int, double> value) {
  match (value) {
    case { int payload } => use(payload);
    case _ => ;
  }
}
void whole(MaybeInt value) {
  match (value) {
    case auto whole => (void)whole;
  }
}
struct Product {
  int first;
  int second;
};
void structured(Product value) {
  match (value) {
    case auto [first, second] => use(first + second);
  }
}
void decomposed(Product value) {
  match (value) {
    case [auto first, auto second] => use(first + second);
  }
}
)cpp");
  ASSERT_TRUE(AST);
  const struct {
    const char *Name;
    bool RequiresRegion;
  } Cases[] = {{"singleton", true},      {"multiple", true},
               {"repeated", true},       {"selected", true},
               {"omitted", true},        {"concrete", false},
               {"whole", true},          {"structured", true},
               {"decomposed", true},     {"value_or", false},
               {"bound_or", true},       {"already_dependent", true},
               {"dependent_whole", true}};
  for (auto [Name, RequiresRegion] : Cases) {
    SCOPED_TRACE(Name);
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Selection, nullptr);
    EXPECT_EQ(Finder.Selection->getCases()[0].Pattern->requiresTemplateRegion(),
              RequiresRegion);
    MatchTemplateRegionDecl *Region = Finder.Selection->getCases()[0].Region;
    if (RequiresRegion) {
      ASSERT_NE(Region, nullptr);
      EXPECT_TRUE(Region->isDependentContext());
      EXPECT_EQ(Region->getDeclContext(), Function);
      EXPECT_EQ(Region->getEnclosingFunction(), Function);
    } else {
      EXPECT_EQ(Region, nullptr);
    }
  }
}

TEST(MatchAST, OrBindingInterfacesControlTemplateRegions) {
  auto AST = buildMatchAST(R"cpp(
template <class First, class Second> struct Product {
  First first;
  Second second;
};
void same(Product<int, int> value) {
  match (value) {
    case [int payload, 0] or [0, int payload] => use(payload);
    case _ => ;
  }
}
void different(Product<int, double> value) {
  match (value) {
    case [int payload, 0] or [0, double payload] => use(payload);
    case _ => ;
  }
}
void nested_same(Product<int, int> value) {
  match (value) {
    case ([int payload, 0] or [0, int payload]) or [int payload, 1] => use(payload);
    case _ => ;
  }
}
void nested_different(Product<int, double> value) {
  match (value) {
    case ([int payload, 0] or [0, double payload]) or [int payload, 1] => use(payload);
    case _ => ;
  }
}
void placeholder_same(Product<int, int> value) {
  match (value) {
    case [auto payload, 0] or [0, auto payload] => use(payload);
    case _ => ;
  }
}
void placeholder_different(Product<int, double> value) {
  match (value) {
    case [auto payload, 0] or [0, auto payload] => use(payload);
    case _ => ;
  }
}
void pack(Product<int, int> value) {
  match (value) {
    case [0, auto ...payload] or [auto ...payload, 0] => (use(payload), ...);
    case _ => ;
  }
}
template <class T> void enclosing_template(Product<T, T> value) {
  match (value) {
    case [T payload, 0] or [0, T payload] => use(payload);
    case _ => ;
  }
}
void qualifiers(Product<int&, const int&> value) {
  match (value) {
    case [int& payload, 0] or [0, const int& payload] => use(payload);
    case _ => ;
  }
}
void no_bindings(Product<int, double> value) {
  match (value) {
    case [int, 0] or [0, double] => ;
    case _ => ;
  }
}
)cpp");
  ASSERT_TRUE(AST);
  const struct {
    const char *Name;
    bool HasCommonBindingTypes;
    unsigned NumInstantiations = 2;
  } Cases[] = {{"same", true},
               {"different", false},
               {"nested_same", true, 3},
               {"nested_different", false, 3},
               {"placeholder_same", false},
               {"placeholder_different", false},
               {"pack", false},
               {"enclosing_template", true},
               {"qualifiers", false},
               {"no_bindings", true}};
  for (auto [Name, HasCommonBindingTypes, NumInstantiations] : Cases) {
    SCOPED_TRACE(Name);
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Selection, nullptr);
    const MatchCase &Case = Finder.Selection->getCases()[0];
    const auto *Or = dyn_cast<OrPattern>(Case.Pattern);
    ASSERT_NE(Or, nullptr);
    EXPECT_EQ(Or->hasCommonBindingTypes(), HasCommonBindingTypes);
    EXPECT_EQ(Or->requiresTemplateRegion(), !HasCommonBindingTypes);
    EXPECT_EQ(Case.Region != nullptr, !HasCommonBindingTypes);
    if (!Or->bindings().empty()) {
      EXPECT_EQ(Or->bindings()[0]->getType()->isDependentType(),
                !HasCommonBindingTypes ||
                    llvm::StringRef(Name) == "enclosing_template");
    }
    if (llvm::StringRef(Name) == "enclosing_template" ||
        llvm::StringRef(Name) == "no_bindings")
      continue;
    ArrayRef<MatchCaseInstantiation> Instantiations = Case.getInstantiations();
    ASSERT_EQ(Instantiations.size(), NumInstantiations);
    for (const auto &Instantiation : Instantiations.drop_front())
      EXPECT_EQ(Instantiation.Handler == Instantiations.front().Handler,
                HasCommonBindingTypes);
    for (const auto &Instantiation : Instantiations)
      EXPECT_EQ(Instantiation.PatternInstantiation->bindingAliases().size(),
                HasCommonBindingTypes ? 1u : 0u);
  }
}

TEST(MatchAST, SelectionPlanningDistinguishesPatternChoices) {
  const struct {
    const char *Pattern;
    bool RequiresSelection;
  } Cases[] = {{"_", false},
               {"0 or 1", false},
               {"(0 or 1) or (2 or 3)", false},
               {"int", false},
               {"[0 or 1, int payload]", false},
               {"[int payload, 0 or 1]", false},
               {"[0 or 1, [int payload, _]]", false},
               {"[[int or long, _], _]", true},
               {"(int or long) or double", true},
               {"{ int payload }", true},
               {"{ .value: int payload }", false},
               {"{ .value: auto payload }", false},
               {"{ .index<0>: int payload }", false},
               {"{ int: _ }", true},
               {"{ Any: _ }", true},
               {"{ .value: [int or long, _] }", true},
               {"{}", false}};
  for (auto [Pattern, RequiresSelection] : Cases) {
    SCOPED_TRACE(Pattern);
    auto AST = buildMatchAST("template <class> concept Any = true;\n"
                             "template <class T> void test(T value) {\n"
                             "  match (value) { case " +
                             std::string(Pattern) + " => ; }\n}");
    ASSERT_TRUE(AST);
    FunctionDecl *Function = getFunction(*AST, "test");
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Selection, nullptr);
    ASSERT_EQ(Finder.Selection->getCases().size(), 1u);
    EXPECT_EQ(Sema::MatchAlternativeSelectionPlan::isRequired(
                  Finder.Selection->getCases()[0].Pattern),
              RequiresSelection);
  }
}

TEST(MatchAST, SelectionPlanningEnumeratesNestedChoices) {
  Sema::MatchAlternativeSelectionPlan Plan;
  Sema::MatchPatternCheckingContext CheckingContext;
  const unsigned FirstAlternatives[] = {2, 5};
  const unsigned SecondAlternatives[] = {3, 7};
  std::vector<std::vector<unsigned>> Selections;
  do {
    Plan.beginCandidate(CheckingContext, /*IsExpandingCandidate=*/true);
    std::optional<unsigned> First =
        CheckingContext.selectAlternative(FirstAlternatives);
    ASSERT_TRUE(First);
    std::optional<unsigned> Second =
        CheckingContext.selectAlternative(SecondAlternatives);
    ASSERT_TRUE(Second);
    Selections.push_back({*First, *Second});
    EXPECT_EQ(CheckingContext.AlternativeChoices.size(), 2u);
    EXPECT_EQ(CheckingContext.NextPlannedAlternativeSelection,
              Plan.Selections.size());
  } while (Plan.advance(CheckingContext));

  const std::vector<std::vector<unsigned>> Expected = {
      {2, 3}, {2, 7}, {5, 3}, {5, 7}};
  EXPECT_EQ(Selections, Expected);
  EXPECT_TRUE(Plan.SawAlternativeChoice);
}

TEST(MatchAST, SelectionPlanningRejectsUnavailableChoices) {
  Sema::MatchAlternativeSelectionPlan Plan;
  Plan.Selections = {2, 7};
  Sema::MatchPatternCheckingContext CheckingContext;
  Plan.beginCandidate(CheckingContext, /*IsExpandingCandidate=*/true);

  const unsigned FirstAlternatives[] = {2, 5};
  const unsigned SecondAlternatives[] = {3};
  EXPECT_EQ(CheckingContext.selectAlternative(FirstAlternatives), 2u);
  EXPECT_FALSE(CheckingContext.selectAlternative(SecondAlternatives));
  EXPECT_EQ(CheckingContext.NextPlannedAlternativeSelection, 2u);
  ASSERT_EQ(CheckingContext.AlternativeChoices.size(), 1u);
  EXPECT_EQ(CheckingContext.AlternativeChoices.front().Selected, 2u);

  ASSERT_TRUE(Plan.advance(CheckingContext));
  EXPECT_EQ(Plan.Selections, (SmallVector<unsigned, 4>{5}));
  Plan.beginCandidate(CheckingContext, /*IsExpandingCandidate=*/true);
  EXPECT_TRUE(CheckingContext.AlternativeChoices.empty());
  EXPECT_EQ(CheckingContext.NextPlannedAlternativeSelection, 0u);
  EXPECT_EQ(CheckingContext.selectAlternative(FirstAlternatives), 5u);
  EXPECT_EQ(CheckingContext.selectAlternative(SecondAlternatives), 3u);
  EXPECT_FALSE(Plan.advance(CheckingContext));
}

TEST(MatchAST, CandidateCheckingKeepsProjectionsButResetsSelectionState) {
  Sema::MatchAlternativeSelectionPlan Plan;
  Plan.Selections = {3};
  Sema::MatchPatternCheckingContext CheckingContext;
  CheckingContext.Projections.Entries.push_back(
      {nullptr, nullptr, QualType(), 0, {}, {}});
  CheckingContext.CurrentProjectionPath = {1, 2};
  CheckingContext.CurrentDiscriminatorPath = {1};
  CheckingContext.CandidateUsefulness.CaseIndex = 7;

  Plan.beginCandidate(CheckingContext, /*IsExpandingCandidate=*/true);
  EXPECT_TRUE(CheckingContext.isCheckingCandidate());
  EXPECT_FALSE(CheckingContext.isDeferring());
  EXPECT_EQ(CheckingContext.Projections.Entries.size(), 1u);
  EXPECT_TRUE(CheckingContext.CurrentProjectionPath.empty());
  EXPECT_TRUE(CheckingContext.CurrentDiscriminatorPath.empty());
  EXPECT_EQ(CheckingContext.CandidateUsefulness.CaseIndex, 0u);

  const unsigned Alternatives[] = {3, 5};
  ASSERT_EQ(CheckingContext.selectAlternative(Alternatives), 3u);
  ASSERT_TRUE(Plan.advance(CheckingContext));
  Plan.beginCandidate(CheckingContext, /*IsExpandingCandidate=*/false);
  EXPECT_FALSE(CheckingContext.isCheckingCandidate());
  EXPECT_TRUE(CheckingContext.AlternativeChoices.empty());
  EXPECT_EQ(CheckingContext.selectAlternative(Alternatives), 5u);
  EXPECT_EQ(CheckingContext.Projections.Entries.size(), 1u);
}

TEST(MatchAST, TentativePatternCheckingRollsBackRecordsAndChoices) {
  Sema::MatchPatternCheckingContext CheckingContext;
  Sema::MatchPatternState State;
  const unsigned Alternatives[] = {3, 5};
  const unsigned Selections[] = {3, 5};
  CheckingContext.PlannedAlternativeSelections = Selections;
  ASSERT_EQ(CheckingContext.selectAlternative(Alternatives), 3u);
  State.Infos.emplace_back();

  {
    Sema::MatchPatternCheckingTransaction Transaction(&CheckingContext, &State);
    CheckingContext.Projections.Entries.push_back(
        {nullptr, nullptr, QualType(), 0, {}, {}});
    State.Infos.emplace_back();
    ASSERT_EQ(CheckingContext.selectAlternative(Alternatives), 5u);
  }
  EXPECT_TRUE(CheckingContext.Projections.Entries.empty());
  EXPECT_EQ(State.Infos.size(), 1u);
  EXPECT_EQ(CheckingContext.AlternativeChoices.size(), 1u);
  EXPECT_EQ(CheckingContext.NextPlannedAlternativeSelection, 1u);
}

TEST(MatchAST, RejectedCandidateKeepsChoicesForEnumeration) {
  Sema::MatchPatternCheckingContext CheckingContext;
  Sema::MatchPatternState State;
  Sema::MatchAlternativeSelectionPlan Plan;
  const unsigned Alternatives[] = {3, 5};
  {
    Sema::MatchPatternCheckingTransaction Transaction(&CheckingContext, &State);
    CheckingContext.Projections.Entries.push_back(
        {nullptr, nullptr, QualType(), 0, {}, {}});
    State.Infos.emplace_back();
    ASSERT_EQ(CheckingContext.selectAlternative(Alternatives), 3u);
    Transaction.commitAlternativeChoices();
  }
  EXPECT_TRUE(CheckingContext.Projections.Entries.empty());
  EXPECT_TRUE(State.Infos.empty());
  ASSERT_TRUE(Plan.advance(CheckingContext));
  EXPECT_EQ(Plan.Selections, (SmallVector<unsigned, 4>{5}));
}

TEST(MatchAST, SuccessfulPatternCheckingCommitsRecords) {
  Sema::MatchPatternCheckingContext CheckingContext;
  Sema::MatchPatternState State;
  {
    Sema::MatchPatternCheckingTransaction Transaction(&CheckingContext, &State);
    CheckingContext.Projections.Entries.push_back(
        {nullptr, nullptr, QualType(), 0, {}, {}});
    State.Infos.emplace_back();
    Transaction.commit();
  }
  EXPECT_EQ(CheckingContext.Projections.Entries.size(), 1u);
  EXPECT_EQ(State.Infos.size(), 1u);
}

TEST(MatchAST, SharedHandlersUseCheckedBindingCorrespondence) {
  auto AST = buildMatchAST(R"cpp(
void test(Choice<int, int>& value) {
  match (value) {
    case { int& payload } => use(payload);
  }
}
)cpp");
  ASSERT_TRUE(AST);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(getFunction(*AST, "test")));
  ASSERT_NE(Finder.Selection, nullptr);
  auto Candidates = Finder.Selection->getCaseInstantiations();
  ASSERT_EQ(Candidates.size(), 2u);
  const ValueDecl *HandlerBinding = nullptr;
  for (const MatchCaseInstantiation &Candidate : Candidates) {
    EXPECT_EQ(Candidate.Handler, Candidates.front().Handler);
    SmallVector<ValueDecl *, 1> Bindings;
    visitMatchPatternBindings(
        Candidate.Pattern, Candidate.PatternInstantiation,
        [&](ValueDecl *Binding) { Bindings.push_back(Binding); });
    ASSERT_EQ(Bindings.size(), 1u);
    const ValueDecl *Declaration =
        Candidate.PatternInstantiation->getBindingDeclaration(Bindings.front());
    if (!HandlerBinding)
      HandlerBinding = Declaration;
    EXPECT_EQ(Declaration, HandlerBinding);
  }
}

TEST(MatchAST, EmptyInstantiationTable) {
  auto AST = buildMatchAST("int value;");
  ASSERT_TRUE(AST);
  auto *Selection = MatchSelectExpr::CreateEmpty(
      AST->getASTContext(), /*NumPreambleStatements=*/0, /*NumCases=*/1,
      /*NumCaseInstantiations=*/0);
  EXPECT_TRUE(Selection->getCaseInstantiations().empty());
}

TEST(MatchAST, SelectionSourceAndInstantiationTraversal) {
  auto AST = buildMatchAST(R"cpp(
void test(Choice<int, double> value) {
  match (value) {
    case { auto payload } => {
      int marker = 0;
      use(payload + marker);
    }
  }
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchVisitor SourceVisitor(false);
  ASSERT_TRUE(SourceVisitor.TraverseDecl(Function));
  EXPECT_EQ(SourceVisitor.Counts.Payloads, 1u);
  EXPECT_EQ(SourceVisitor.Counts.DependentPayloads, 1u);
  EXPECT_EQ(SourceVisitor.Counts.Markers, 1u);
  EXPECT_EQ(SourceVisitor.Counts.Regions, 1u);

  MatchVisitor InstantiationVisitor(true);
  ASSERT_TRUE(InstantiationVisitor.TraverseDecl(Function));
  EXPECT_EQ(InstantiationVisitor.Counts.Payloads, 3u);
  EXPECT_EQ(InstantiationVisitor.Counts.DependentPayloads, 1u);
  EXPECT_EQ(InstantiationVisitor.Counts.Markers, 3u);
  EXPECT_EQ(InstantiationVisitor.Counts.Regions, 1u);
}

TEST(MatchAST, DirectCaseIsNotVisitedTwice) {
  auto AST = buildMatchAST(R"cpp(
int test(int value) {
  return match (value) {
    case int payload => payload;
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchVisitor Visitor(true);
  ASSERT_TRUE(Visitor.TraverseDecl(Function));
  EXPECT_EQ(Visitor.Counts.Payloads, 1u);
  EXPECT_EQ(Visitor.Counts.Regions, 0u);
}

TEST(MatchAST, CompoundPatternsTraverseEachChild) {
  auto AST = buildMatchAST(R"cpp(
int test(int (&value)[2]) {
  return match (value) {
    case [(0 or 1), int payload] and _ => payload;
    case _ => 0;
  };
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  ASSERT_NE(Finder.Selection, nullptr);

  class PatternVisitor : public RecursiveASTVisitor<PatternVisitor> {
  public:
    std::vector<MatchPattern::MatchPatternClass> Classes;
    bool TraverseMatchPattern(MatchPattern *Pattern) {
      if (Pattern)
        Classes.push_back(Pattern->getMatchPatternClass());
      return RecursiveASTVisitor::TraverseMatchPattern(Pattern);
    }
  } Visitor;
  ASSERT_TRUE(
      Visitor.TraverseMatchPattern(Finder.Selection->getCases()[0].Pattern));
  EXPECT_EQ(Visitor.Classes,
            (std::vector<MatchPattern::MatchPatternClass>{
                MatchPattern::AndPatternClass,
                MatchPattern::DecompositionPatternClass,
                MatchPattern::ParenPatternClass, MatchPattern::OrPatternClass,
                MatchPattern::ExpressionPatternClass,
                MatchPattern::ExpressionPatternClass,
                MatchPattern::DeclarationPatternClass,
                MatchPattern::WildcardPatternClass}));
}

TEST(MatchAST, TestPreservesSourcePattern) {
  auto AST = buildMatchAST(R"cpp(
bool test(Choice<int, double> value) {
  return match(value, case { auto payload } if (keep(payload)));
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  ASSERT_NE(Finder.Test, nullptr);
  EXPECT_NE(Finder.Test->getSourceExpr(), Finder.Test);
  EXPECT_EQ(Finder.Test->getInstantiations().size(), 2u);
  EXPECT_EQ(Finder.Test->getRParenLoc(),
            Finder.Test->getSourceExpr()->getRParenLoc());
  EXPECT_EQ(Finder.Test->getRegion(),
            Finder.Test->getSourceExpr()->getRegion());

  MatchVisitor SourceVisitor(false);
  ASSERT_TRUE(SourceVisitor.TraverseDecl(Function));
  EXPECT_EQ(SourceVisitor.Counts.Payloads, 1u);
  EXPECT_EQ(SourceVisitor.Counts.DependentPayloads, 1u);
  MatchVisitor InstantiationVisitor(true);
  ASSERT_TRUE(InstantiationVisitor.TraverseDecl(Function));
  EXPECT_EQ(InstantiationVisitor.Counts.Payloads, 3u);
  EXPECT_EQ(InstantiationVisitor.Counts.DependentPayloads, 1u);

  std::string Printed;
  llvm::raw_string_ostream OS(Printed);
  Finder.Test->printPretty(OS, nullptr,
                           AST->getASTContext().getPrintingPolicy());
  EXPECT_NE(Printed.find("auto payload"), std::string::npos);
}

TEST(MatchAST, ConditionPreservesSourceContinuation) {
  auto AST = buildMatchAST(R"cpp(
void test(Choice<int, double> value) {
  if (case { auto payload } = value && keep(payload)) {
    int marker = 0;
    use(payload + marker);
  }
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchFinder Finder;
  ASSERT_TRUE(Finder.TraverseDecl(Function));
  ASSERT_NE(Finder.Test, nullptr);
  ASSERT_EQ(Finder.Test->getSourceContinuation().size(), 1u);
  EXPECT_NE(Finder.Test->getSourceHandler(), nullptr);
  MatchVisitor SourceVisitor(false);
  ASSERT_TRUE(SourceVisitor.TraverseDecl(Function));
  EXPECT_EQ(SourceVisitor.Counts.Payloads, 1u);
  EXPECT_EQ(SourceVisitor.Counts.DependentPayloads, 1u);
  EXPECT_EQ(SourceVisitor.Counts.Markers, 1u);
  MatchVisitor InstantiationVisitor(true);
  ASSERT_TRUE(InstantiationVisitor.TraverseDecl(Function));
  EXPECT_EQ(InstantiationVisitor.Counts.Payloads, 3u);
  EXPECT_EQ(InstantiationVisitor.Counts.DependentPayloads, 1u);
  EXPECT_EQ(InstantiationVisitor.Counts.Markers, 3u);

  Expr *Condition = Finder.Test->getSourceContinuation().front();
  Expr *Continuation[] = {Condition};
  Finder.Test->setSourceForm(AST->getASTContext(), Finder.Test->getSourceExpr(),
                             Continuation, Finder.Test->getSourceHandler(),
                             Finder.Test->getSourceIncrement());
  EXPECT_NE(Finder.Test->getSourceContinuation().data(), Continuation);
  Continuation[0] = nullptr;
  EXPECT_EQ(Finder.Test->getSourceContinuation().front(), Condition);
}

TEST(MatchAST, GuardComponentsAreVisitedOncePerForm) {
  auto AST = buildMatchAST(R"cpp(
bool test(Choice<int, double> value) {
  return match (value, case { auto payload }
      if (int marker = 0; auto guard = keep(payload + marker)));
}
void selection(Choice<int, double> value) {
  match (value) {
    case { auto payload }
        if (int marker = 0; auto guard = keep(payload + marker)) => use(payload);
    case _ => ;
  }
}
)cpp");
  ASSERT_TRUE(AST);
  auto CheckCounts = [](const VisitCounts &Counts, unsigned Forms) {
    EXPECT_EQ(Counts.Payloads, Forms);
    EXPECT_EQ(Counts.Markers, Forms);
    EXPECT_EQ(Counts.Guards, Forms);
  };
  for (llvm::StringRef Name : {"test", "selection"}) {
    SCOPED_TRACE(Name.str());
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchVisitor SourceVisitor(false);
    ASSERT_TRUE(SourceVisitor.TraverseDecl(Function));
    CheckCounts(SourceVisitor.Counts, 1);
    MatchVisitor InstantiationVisitor(true);
    ASSERT_TRUE(InstantiationVisitor.TraverseDecl(Function));
    CheckCounts(InstantiationVisitor.Counts, 3);
    MatchTraverser SourceTraverser;
    SourceTraverser.SetTraversalKind(TK_IgnoreUnlessSpelledInSource);
    SourceTraverser.Visit(Function);
    CheckCounts(SourceTraverser.Recorder.Counts, 1);
    MatchTraverser CheckedTraverser;
    CheckedTraverser.Visit(Function);
    CheckCounts(CheckedTraverser.Recorder.Counts, 2);
  }
}

TEST(MatchAST, DirectConditionVisitsItsHandlerOnce) {
  auto AST = buildMatchAST(R"cpp(
void test(int value) {
  if (case int payload = value) {
    int marker = 0;
    use(payload + marker);
  }
}

void for_condition(int value) {
  for (; case int payload = value; ++payload) {
    int marker = 0;
    use(payload + marker);
    break;
  }
}
)cpp");
  ASSERT_TRUE(AST);
  for (llvm::StringRef Name : {"test", "for_condition"}) {
    SCOPED_TRACE(Name.str());
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Test, nullptr);
    ASSERT_EQ(Finder.Test->getInstantiations().size(), 1u);
    const MatchTestInstantiation &Instantiation =
        Finder.Test->getInstantiations().front();
    EXPECT_TRUE(Finder.Test->isDirectInstantiation(Instantiation));
    EXPECT_NE(Instantiation.Handler, nullptr);
    if (Name == "for_condition") {
      auto *Loop =
          cast<ForStmt>(cast<CompoundStmt>(Function->getBody())->body_front());
      EXPECT_EQ(Instantiation.Handler, Loop->getBody());
      EXPECT_EQ(Instantiation.Increment, Loop->getInc());
      EXPECT_NE(Instantiation.Increment, nullptr);
    } else {
      EXPECT_EQ(Instantiation.Increment, nullptr);
    }
    MatchVisitor Visitor(true);
    ASSERT_TRUE(Visitor.TraverseDecl(Function));
    EXPECT_EQ(Visitor.Counts.Payloads, 1u);
    EXPECT_EQ(Visitor.Counts.Markers, 1u);
    MatchTraverser Traverser;
    Traverser.Visit(Function);
    EXPECT_EQ(Traverser.Recorder.Counts.Payloads, 1u);
    EXPECT_EQ(Traverser.Recorder.Counts.Markers, 1u);
  }
}

TEST(MatchAST, ResolvedTestsHaveCheckedCandidates) {
  auto AST = buildMatchAST(R"cpp(
bool concrete(int value) { return match(value, case int payload); }
bool wildcard(int value) { return match(value, case _); }
bool optional(MaybeInt value) { return match(value, case { int payload }); }
template <class T> bool dependent(T value) {
  return match(value, case int payload);
}
)cpp");
  ASSERT_TRUE(AST);
  for (llvm::StringRef Name :
       {"concrete", "wildcard", "optional", "dependent"}) {
    SCOPED_TRACE(Name.str());
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(getFunction(*AST, Name)));
    ASSERT_NE(Finder.Test, nullptr);
    if (Name == "dependent") {
      EXPECT_TRUE(Finder.Test->getSubject()->isTypeDependent());
      EXPECT_TRUE(Finder.Test->getInstantiations().empty());
    } else {
      EXPECT_FALSE(Finder.Test->needsCaseInstantiation());
      ASSERT_EQ(Finder.Test->getInstantiations().size(), 1u);
      EXPECT_TRUE(Finder.Test->isDirectInstantiation(
          Finder.Test->getInstantiations().front()));
      EXPECT_EQ(Finder.Test->getRegion(), nullptr);
    }
  }
}

TEST(MatchAST, NodeTraverserSourceAndCheckedForms) {
  auto AST = buildMatchAST(R"cpp(
void test(Choice<int, double> value) {
  match (value) {
    case { auto payload } => {
      int marker = 0;
      use(payload + marker);
    }
  }
}
)cpp");
  ASSERT_TRUE(AST);
  FunctionDecl *Function = getFunction(*AST, "test");
  ASSERT_NE(Function, nullptr);
  MatchTraverser SourceTraverser;
  SourceTraverser.SetTraversalKind(TK_IgnoreUnlessSpelledInSource);
  SourceTraverser.Visit(Function);
  EXPECT_EQ(SourceTraverser.Recorder.Counts.Payloads, 1u);
  EXPECT_EQ(SourceTraverser.Recorder.Counts.DependentPayloads, 1u);
  EXPECT_EQ(SourceTraverser.Recorder.Counts.Markers, 1u);
  EXPECT_EQ(SourceTraverser.Recorder.Counts.Regions, 0u);
  MatchTraverser CheckedTraverser;
  CheckedTraverser.Visit(Function);
  EXPECT_EQ(CheckedTraverser.Recorder.Counts.Payloads, 2u);
  EXPECT_EQ(CheckedTraverser.Recorder.Counts.DependentPayloads, 0u);
  EXPECT_EQ(CheckedTraverser.Recorder.Counts.Markers, 2u);
  EXPECT_EQ(CheckedTraverser.Recorder.Counts.Regions, 1u);
}

TEST(MatchAST, NodeTraverserTestSourceAndCheckedForms) {
  auto AST = buildMatchAST(R"cpp(
bool test(Choice<int, double> value) {
  return match(value, case { auto payload } if (keep(payload)));
}
void condition(Choice<int, double> value) {
  if (case { auto payload } = value && keep(payload)) {
    int marker = 0;
    use(payload + marker);
  }
}
)cpp");
  ASSERT_TRUE(AST);
  for (llvm::StringRef Name : {"test", "condition"}) {
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchTraverser SourceTraverser;
    SourceTraverser.SetTraversalKind(TK_IgnoreUnlessSpelledInSource);
    SourceTraverser.Visit(Function);
    EXPECT_EQ(SourceTraverser.Recorder.Counts.Payloads, 1u);
    EXPECT_EQ(SourceTraverser.Recorder.Counts.DependentPayloads, 1u);
    EXPECT_EQ(SourceTraverser.Recorder.Counts.Regions, 0u);
    MatchTraverser CheckedTraverser;
    CheckedTraverser.Visit(Function);
    EXPECT_EQ(CheckedTraverser.Recorder.Counts.Payloads, 2u);
    EXPECT_EQ(CheckedTraverser.Recorder.Counts.DependentPayloads, 0u);
    EXPECT_EQ(CheckedTraverser.Recorder.Counts.Regions, 1u);
    if (Name == "condition") {
      EXPECT_EQ(SourceTraverser.Recorder.Counts.Markers, 1u);
      EXPECT_EQ(CheckedTraverser.Recorder.Counts.Markers, 2u);
    }
  }
}

TEST(MatchAST, DeferredDumperKeepsMatchState) {
  auto AST = buildMatchAST(R"cpp(
bool singleton(MaybeInt value) {
  return match (value, case { int payload } if (keep(payload)));
}
bool multiple(Choice<int, double> value) {
  return match (value, case { auto payload } if (keep(payload)));
}
void selection(Choice<int, double> value) {
  match (value) {
    case { auto payload } => use(payload);
  }
}
)cpp");
  ASSERT_TRUE(AST);
  for (llvm::StringRef Name : {"singleton", "multiple", "selection"}) {
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    std::string Dump;
    llvm::raw_string_ostream OS(Dump);
    Function->dump(OS);
    EXPECT_NE(
        Dump.find(Name == "selection" ? "MatchSelectExpr" : "MatchTestExpr"),
        std::string::npos);
    EXPECT_NE(Dump.find("payload"), std::string::npos);
  }
}

} // namespace
