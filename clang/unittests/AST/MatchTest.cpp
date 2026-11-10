//===- unittests/AST/MatchTest.cpp - Match AST tests
//-------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

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

} // namespace
