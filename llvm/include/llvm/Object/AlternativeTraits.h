//===- AlternativeTraits.h - Object pattern matching support ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECT_ALTERNATIVETRAITS_H
#define LLVM_OBJECT_ALTERNATIVETRAITS_H

#include "llvm/Object/Binary.h"
#include "llvm/Support/Casting.h"
#include <type_traits>
#include <utility>

#ifdef __clang__
#if __has_feature(pattern_matching)

namespace std {

template <> struct alternative_traits<llvm::object::Binary> {
  template <class T, bool IsLvalue> struct result {
    T *value;

    explicit operator bool() const noexcept { return value != nullptr; }

    decltype(auto) operator*() && {
      if constexpr (IsLvalue)
        return static_cast<T &>(*value);
      else
        return static_cast<T &&>(*value);
    }
  };

  template <class T, class Self> static auto try_cast(Self &&Value) {
    using Target = remove_reference_t<T>;
    auto *Cast = llvm::dyn_cast<Target>(&Value);
    using Projected = remove_pointer_t<decltype(Cast)>;
    return result<Projected, is_lvalue_reference_v<Self &&>>{Cast};
  }

  template <class T, class Self> static auto try_init(Self &&Value) {
    return try_cast<T>(std::forward<Self>(Value));
  }
};

} // namespace std

#endif // __has_feature(pattern_matching)
#endif // __clang__

#endif // LLVM_OBJECT_ALTERNATIVETRAITS_H
