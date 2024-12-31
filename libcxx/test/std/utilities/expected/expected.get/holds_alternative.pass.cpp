//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// <expected>

// template <class U, class T, class E>
// constexpr bool holds_alternative(const expected<T, E>& v) noexcept;

#include "test_macros.h"
#include <expected>

int main(int, char**) {
  {
    using E = std::expected<int, long>;
    constexpr E e(42);
    static_assert(std::holds_alternative<int>(e), "");
    static_assert(!std::holds_alternative<long>(e), "");
  }
  {
    using E = std::expected<int, long>;
    constexpr E e(std::unexpect, 42l);
    static_assert(!std::holds_alternative<int>(e), "");
    static_assert(std::holds_alternative<long>(e), "");
  }
  {
    using E = std::expected<int, long>;
    constexpr E e;
    static_assert(std::holds_alternative<int>(e), "");
    static_assert(!std::holds_alternative<long>(e), "");
  }
  {
    using E = std::expected<int, int>;
    constexpr E e(42);
    static_assert(std::holds_alternative<int>(e), "");
  }
  {
    using E = std::expected<int, int>;
    constexpr E e(std::unexpect, 42);
    static_assert(std::holds_alternative<int>(e), "");
  }
  { // noexcept test
    using E = std::expected<int, long>;
    const E e;
    ASSERT_NOEXCEPT(std::holds_alternative<int>(e));
  }
  { // noexcept test
    using E = std::expected<int, int>;
    const E e;
    ASSERT_NOEXCEPT(std::holds_alternative<int>(e));
  }

  return 0;
}
