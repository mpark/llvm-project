//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// <expected>

//  template <class U, class T, class E>
//  constexpr add_pointer_t<U> get_if(expected<T, E>* e) noexcept;
// template <class U, class T, class E>
//  constexpr add_pointer_t<const U> get_if(const expected<T, E>* e)
//  noexcept;

#include "test_macros.h"
#include "variant_test_helpers.h"
#include <cassert>
#include <expected>

void test_const_get_if() {
  {
    using E = std::expected<int, long>;
    constexpr const E *e = nullptr;
    static_assert(std::get_if<int>(e) == nullptr, "");
  }
  {
    using E = std::expected<int, long>;
    constexpr E e(42);
    ASSERT_NOEXCEPT(std::get_if<int>(&e));
    ASSERT_SAME_TYPE(decltype(std::get_if<int>(&e)), const int *);
    static_assert(*std::get_if<int>(&e) == 42, "");
    static_assert(std::get_if<long>(&e) == nullptr, "");
  }
  {
    using E = std::expected<int, long>;
    constexpr E e(std::unexpect, 42l);
    ASSERT_SAME_TYPE(decltype(std::get_if<long>(&e)), const long *);
    static_assert(*std::get_if<long>(&e) == 42, "");
    static_assert(std::get_if<int>(&e) == nullptr, "");
  }
  {
    using E = std::expected<int, int>;
    constexpr const E *e = nullptr;
    static_assert(std::get_if<int>(e) == nullptr, "");
  }
  {
    using E = std::expected<int, int>;
    constexpr E e(42);
    ASSERT_NOEXCEPT(std::get_if<int>(&e));
    ASSERT_SAME_TYPE(decltype(std::get_if<int>(&e)), const int *);
    static_assert(*std::get_if<int>(&e) == 42, "");
  }
  {
    using E = std::expected<int, int>;
    constexpr E e(std::unexpect, 42);
    ASSERT_SAME_TYPE(decltype(std::get_if<long>(&e)), const long *);
    static_assert(*std::get_if<int>(&e) == 42, "");
  }
}

void test_get_if() {
  {
    using E = std::expected<int, long>;
    E *e = nullptr;
    assert(std::get_if<int>(e) == nullptr);
  }
  {
    using E = std::expected<int, long>;
    E e(42);
    ASSERT_NOEXCEPT(std::get_if<int>(&e));
    ASSERT_SAME_TYPE(decltype(std::get_if<int>(&e)), int *);
    assert(*std::get_if<int>(&e) == 42);
    assert(std::get_if<long>(&e) == nullptr);
  }
  {
    using E = std::expected<int, long>;
    E e(std::unexpect, 42l);
    ASSERT_SAME_TYPE(decltype(std::get_if<long>(&e)), long *);
    assert(*std::get_if<long>(&e) == 42);
    assert(std::get_if<int>(&e) == nullptr);
  }
  {
    using E = std::expected<int, int>;
    E *e = nullptr;
    assert(std::get_if<int>(e) == nullptr);
  }
  {
    using E = std::expected<int, int>;
    E e(42);
    ASSERT_NOEXCEPT(std::get_if<int>(&e));
    ASSERT_SAME_TYPE(decltype(std::get_if<int>(&e)), int *);
    assert(*std::get_if<int>(&e) == 42);
  }
  {
    using E = std::expected<int, int>;
    E e(std::unexpect, 42);
    ASSERT_SAME_TYPE(decltype(std::get_if<int>(&e)), int *);
    assert(*std::get_if<int>(&e) == 42);
  }
}

int main(int, char**) {
  test_const_get_if();
  test_get_if();

  return 0;
}
