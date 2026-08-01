//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

#include <cassert>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

template <class _Traits, std::size_t _Ip, class _Tp>
constexpr bool alternative_is = std::is_same_v<typename _Traits::template projection_type<_Ip>, _Tp>;

template <class _Traits, std::size_t _Ip>
concept has_projection = requires { typename _Traits::template projection_type<_Ip>; };

template <class _Tp>
concept has_alternative_traits = requires { std::alternative_traits<_Tp>::size; };

static_assert(!has_alternative_traits<int*>);

constexpr bool test_optional() {
  using Traits = std::alternative_traits<std::optional<int>>;
  static_assert(Traits::size == 2);
  static_assert(Traits::is_exhaustive);
  static_assert(noexcept(Traits::index(std::declval<const std::optional<int>&>())));
  static_assert(!has_projection<Traits, 0>);
  static_assert(alternative_is<Traits, 1, int>);
  static_assert(has_projection<Traits, 1>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<std::optional<int>&>())), int&>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<std::optional<int>&&>())), int&&>);

  std::optional<int> value = 42;
  assert(Traits::index(value) == 1);
  assert(Traits::get<1>(value) == 42);
  value.reset();
  assert(Traits::index(value) == 0);
  return true;
}

constexpr bool test_variant() {
  using Variant = std::variant<int, double>;
  using Traits  = std::alternative_traits<Variant>;
  static_assert(Traits::size == 2);
  static_assert(!Traits::is_exhaustive);
  static_assert(noexcept(Traits::index(std::declval<const Variant&>())));
  static_assert(alternative_is<Traits, 0, int>);
  static_assert(alternative_is<Traits, 1, double>);
  static_assert(has_projection<Traits, 0>);
  static_assert(has_projection<Traits, 1>);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<Variant&>())), int&>);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<const Variant&>())), const int&>);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<Variant&&>())), int&&>);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<const Variant&&>())), const int&&>);
  static_assert(noexcept(Traits::get<0>(std::declval<Variant&>())));

  Variant value = 42;
  assert(Traits::index(value) == 0);
  assert(Traits::get<0>(value) == 42);
  value = 2.5;
  assert(Traits::index(value) == 1);
  assert(Traits::get<1>(value) == 2.5);
  return true;
}

constexpr bool test_expected() {
  using Expected = std::expected<int, long>;
  using Traits   = std::alternative_traits<Expected>;
  static_assert(Traits::size == 2);
  static_assert(Traits::is_exhaustive);
  static_assert(noexcept(Traits::index(std::declval<const Expected&>())));
  static_assert(alternative_is<Traits, 0, int>);
  static_assert(alternative_is<Traits, 1, long>);
  static_assert(has_projection<Traits, 0>);
  static_assert(has_projection<Traits, 1>);
  static_assert(Traits::names::value == 0);
  static_assert(Traits::names::error == 1);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<Expected&>())), int&>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<Expected&&>())), long&&>);
  using VoidTraits = std::alternative_traits<std::expected<void, long>>;
  static_assert(alternative_is<VoidTraits, 0, void>);
  static_assert(has_projection<VoidTraits, 0>);
  static_assert(std::is_same_v<decltype(VoidTraits::get<0>(std::declval<std::expected<void, long>&>())), void>);

  Expected value = 42;
  assert(Traits::index(value) == Traits::names::value);
  assert(Traits::get<0>(value) == 42);
  value = std::unexpected(7L);
  assert(Traits::index(value) == Traits::names::error);
  assert(Traits::get<1>(value) == 7);
  return true;
}

int main(int, char**) {
  static_assert(test_optional());
  static_assert(test_variant());
  static_assert(test_expected());
  return 0;
}
