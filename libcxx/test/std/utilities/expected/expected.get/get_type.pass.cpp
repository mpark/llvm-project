//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// <expected>

// template <class T, class T, class E> constexpr T& get(expected<T, E>& v);
// template <class T, class T, class E> constexpr T&& get(expected<T, E>&& v);
// template <class T, class T, class E> constexpr const T& get(const
// expected<T, E>& v);
// template <class T, class T, class E> constexpr const T&& get(const
// expected<T, E>&& v);

#include "test_macros.h"
#include "test_workarounds.h"
#include "variant_test_helpers.h"
#include <cassert>
#include <type_traits>
#include <utility>
#include <expected>

void test_const_lvalue_get() {
  {
    using E = std::expected<int, long>;
    constexpr E e(42);
    ASSERT_NOT_NOEXCEPT(std::get<int>(e));
    ASSERT_SAME_TYPE(decltype(std::get<int>(e)), const int &);
    static_assert(std::get<int>(e) == 42, "");
  }
  {
    using E = std::expected<int, long>;
    const E e(42);
    ASSERT_NOT_NOEXCEPT(std::get<int>(e));
    ASSERT_SAME_TYPE(decltype(std::get<int>(e)), const int &);
    assert(std::get<int>(e) == 42);
  }
  {
    using E = std::expected<int, long>;
    constexpr E e(std::unexpect, 42l);
    ASSERT_NOT_NOEXCEPT(std::get<long>(e));
    ASSERT_SAME_TYPE(decltype(std::get<long>(e)), const long &);
    static_assert(std::get<long>(e) == 42, "");
  }
  {
    using E = std::expected<int, long>;
    const E e(std::unexpect, 42l);
    ASSERT_NOT_NOEXCEPT(std::get<long>(e));
    ASSERT_SAME_TYPE(decltype(std::get<long>(e)), const long &);
    assert(std::get<long>(e) == 42);
  }
}

void test_lvalue_get() {
  {
    using E = std::expected<int, long>;
    E e(42);
    ASSERT_NOT_NOEXCEPT(std::get<int>(e));
    ASSERT_SAME_TYPE(decltype(std::get<int>(e)), int &);
    assert(std::get<int>(e) == 42);
  }
}

void test_rvalue_get() {
  {
    using E = std::expected<int, long>;
    E e(42);
    ASSERT_NOT_NOEXCEPT(std::get<int>(std::move(e)));
    ASSERT_SAME_TYPE(decltype(std::get<int>(std::move(e))), int &&);
    assert(std::get<int>(std::move(e)) == 42);
  }
}

void test_const_rvalue_get() {
  {
    using E = std::expected<int, long>;
    const E e(42);
    ASSERT_NOT_NOEXCEPT(std::get<int>(std::move(e)));
    ASSERT_SAME_TYPE(decltype(std::get<int>(std::move(e))), const int &&);
    assert(std::get<int>(std::move(e)) == 42);
  }
  {
    using E = std::expected<int, long>;
    const E e(std::unexpect, 42l);
    ASSERT_SAME_TYPE(decltype(std::get<long>(std::move(e))),
                     const long &&);
    assert(std::get<long>(std::move(e)) == 42);
  }
}

template <class Tp> struct identity { using type = Tp; };

void test_throws_for_all_value_categories() {
#ifndef TEST_HAS_NO_EXCEPTIONS
  using E = std::expected<int, long>;
  E e0(42);
  const E &ce0 = e0;
  assert(e0.index() == 0);
  E e1(std::unexpect, 42l);
  const E &ce1 = e1;
  assert(e1.index() == 1);
  identity<int> zero;
  identity<long> one;
  auto test = [](auto idx, auto &&e) {
    using Idx = decltype(idx);
    try {
      TEST_IGNORE_NODISCARD std::get<typename Idx::type>(std::forward<decltype(e)>(e));
    } catch (const std::bad_variant_access &) {
      return true;
    } catch (...) { /* ... */
    }
    return false;
  };
  { // lvalue test cases
    assert(test(one, e0));
    assert(test(zero, e1));
  }
  { // const lvalue test cases
    assert(test(one, ce0));
    assert(test(zero, ce1));
  }
  { // rvalue test cases
    assert(test(one, std::move(e0)));
    assert(test(zero, std::move(e1)));
  }
  { // const rvalue test cases
    assert(test(one, std::move(ce0)));
    assert(test(zero, std::move(ce1)));
  }
#endif
}

int main(int, char**) {
  test_const_lvalue_get();
  test_lvalue_get();
  test_rvalue_get();
  test_const_rvalue_get();
  test_throws_for_all_value_categories();

  return 0;
}
