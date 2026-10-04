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
#include "llvm/Support/Casting.h"
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

} // namespace clang::detail

namespace std {

#include "clang/AST/DeclAlternativeTraits.inc"

} // namespace std

#endif // __has_feature(pattern_matching)
#endif // __clang__

#endif // LLVM_CLANG_AST_ALTERNATIVETRAITS_H
