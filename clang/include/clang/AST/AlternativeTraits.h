//===- AlternativeTraits.h - AST pattern matching support -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_ALTERNATIVETRAITS_H
#define LLVM_CLANG_AST_ALTERNATIVETRAITS_H

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclFriend.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclOpenACC.h"
#include "clang/AST/DeclOpenMP.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprConcepts.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/ExprOpenMP.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/StmtObjC.h"
#include "clang/AST/StmtOpenACC.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/AST/StmtSYCL.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <utility>

#ifdef __clang__
#if __has_feature(pattern_matching)

namespace clang::detail {

template <Decl::Kind Kind> struct DeclForKind;

#define ABSTRACT_DECL(Type)
#define DECL(Derived, Base)                                                    \
  template <> struct DeclForKind<Decl::Derived> {                              \
    using type = Derived##Decl;                                                \
  };
#include "clang/AST/DeclNodes.inc"

template <class Base, Decl::Kind First> struct DeclAlternativeTraitsBase {
  static std::size_t index(const Base &Value) noexcept {
    return static_cast<std::size_t>(Value.getKind()) -
           static_cast<std::size_t>(First);
  }

  template <std::size_t State, class Self>
  static decltype(auto) get(Self &&Value) {
    constexpr auto Kind = static_cast<Decl::Kind>(
        static_cast<std::size_t>(First) + State);
    using Type = typename DeclForKind<Kind>::type;
    using Result =
        decltype(std::forward_like<Self>(std::declval<Type &>()));
    return static_cast<Result>(Value);
  }
};

template <Stmt::StmtClass Class> struct StmtForClass;

#define ABSTRACT_STMT(Type)
#define STMT(Derived, Base)                                                    \
  template <> struct StmtForClass<Stmt::Derived##Class> {                      \
    using type = Derived;                                                      \
  };
#include "clang/AST/StmtNodes.inc"

template <class Base, Stmt::StmtClass First>
struct StmtAlternativeTraitsBase {
  static std::size_t index(const Base &Value) noexcept {
    return static_cast<std::size_t>(Value.getStmtClass()) -
           static_cast<std::size_t>(First);
  }

  template <std::size_t State, class Self>
  static decltype(auto) get(Self &&Value) {
    constexpr auto Class = static_cast<Stmt::StmtClass>(
        static_cast<std::size_t>(First) + State);
    using Type = typename StmtForClass<Class>::type;
    using Result =
        decltype(std::forward_like<Self>(std::declval<Type &>()));
    return static_cast<Result>(Value);
  }
};

} // namespace clang::detail

namespace std {

#include "clang/AST/DeclAlternativeTraits.inc"
#include "clang/AST/StmtAlternativeTraits.inc"

} // namespace std

#endif // __has_feature(pattern_matching)
#endif // __clang__

#endif // LLVM_CLANG_AST_ALTERNATIVETRAITS_H
