//===--- Pattern.h - Classes for representing C++ patterns ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the C++ pattern AST node classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PATTERN_H
#define LLVM_CLANG_AST_PATTERN_H

#include "clang/AST/Stmt.h"

namespace clang {

struct Pattern : public Stmt {
  SourceLocation PatternLoc;

 protected:
  explicit Pattern(StmtClass SC, SourceLocation PatternLoc)
      : Stmt(SC), PatternLoc(PatternLoc) {}

  explicit Pattern(StmtClass SC, EmptyShell) : Stmt(SC) {}

 public:
  SourceLocation getBeginLoc() const { return PatternLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == WildcardPatternClass ||
           T->getStmtClass() == OptionalPatternClass;
  }
};

struct WildcardPattern final : public Pattern {
 public:
  explicit WildcardPattern(SourceLocation WildcardLoc)
      : Pattern(WildcardPatternClass, WildcardLoc) {}

  explicit WildcardPattern(EmptyShell Empty)
      : Pattern(WildcardPatternClass, Empty) {}

  SourceLocation getEndLoc() const { return getBeginLoc(); }

  child_range children() {
    return child_range(child_iterator(), child_iterator());
  }

  const_child_range children() const {
    return const_child_range(const_child_iterator(), const_child_iterator());
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == WildcardPatternClass;
  }
};

struct OptionalPattern final : public Pattern {
  Stmt* SubPattern;

public:
  explicit OptionalPattern(SourceLocation QuestionLoc, Stmt *SubPattern)
      : Pattern(OptionalPatternClass, QuestionLoc), SubPattern(SubPattern) {}

  explicit OptionalPattern(EmptyShell Empty)
      : Pattern(OptionalPatternClass, Empty) {}

  SourceLocation getEndLoc() const { return SubPattern->getEndLoc(); }

  Stmt *getSubPattern() { return SubPattern; }

  child_range children() { return child_range(&SubPattern, &SubPattern + 1); }

  const_child_range children() const {
    return const_child_range(&SubPattern, &SubPattern + 1);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OptionalPatternClass;
  }
};

}  // end namespace clang

#endif
