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

} // namespace
