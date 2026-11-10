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
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <memory>
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
  for (Decl *D : AST.getASTContext().getTranslationUnitDecl()->decls())
    if (auto *Function = dyn_cast<FunctionDecl>(D);
        Function && Function->getName() == Name)
      return Function;
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

TEST(MatchAST, TemplateRegionsCountProjectableStates) {
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
)cpp");
  ASSERT_TRUE(AST);
  for (llvm::StringRef Name : {"singleton", "multiple", "repeated"}) {
    FunctionDecl *Function = getFunction(*AST, Name);
    ASSERT_NE(Function, nullptr);
    MatchFinder Finder;
    ASSERT_TRUE(Finder.TraverseDecl(Function));
    ASSERT_NE(Finder.Selection, nullptr);
    MatchTemplateRegionDecl *Region = Finder.Selection->getCases()[0].Region;
    if (Name == "singleton") {
      EXPECT_EQ(Region, nullptr);
    } else {
      ASSERT_NE(Region, nullptr);
      EXPECT_TRUE(Region->isDependentContext());
      EXPECT_EQ(Region->getEnclosingFunction(), Function);
    }
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
