//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>
#include <chrono>
#include <compare>
#include <expected>
#include <meta>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

template <class _Traits, std::size_t _Ip, class _Self>
concept has_projection = requires(_Self&& __self) {
  _Traits::template get<_Ip>(std::forward<_Self>(__self));
};

template <class _Traits, std::size_t _Ip>
concept has_alternative_info = _Traits::alternatives[_Ip].info != std::meta::info{};

template <class _Tp>
concept has_alternative_traits = requires { std::alternative_traits<_Tp>::alternatives; };

template <class _Traits>
inline constexpr std::size_t alternative_count =
    sizeof(_Traits::alternatives) / sizeof(_Traits::alternatives[0]);

constexpr std::alternative_info index_only_state;
static_assert(index_only_state.info == std::meta::info{});
static_assert(!index_only_state.empty);

static_assert(has_alternative_traits<int*>);
static_assert(!has_alternative_traits<std::unique_ptr<int[]>>);
static_assert(!has_alternative_traits<std::shared_ptr<int[]>>);

constexpr bool test_pointer() {
  using Traits = std::alternative_traits<int*>;
  static_assert(alternative_count<Traits> == 2);
  static_assert(!Traits::has_residual_states);
  static_assert(noexcept(Traits::index(std::declval<int* const&>())));
  static_assert(std::is_same_v<decltype(Traits::index(std::declval<int* const&>())), bool>);
  static_assert(Traits::names::none.index == 0);
  static_assert(Traits::names::some.index == 1);
  static_assert(has_alternative_info<Traits, 0>);
  static_assert(has_alternative_info<Traits, 1>);
  static_assert(std::meta::is_value(Traits::alternatives[0].info));
  static_assert([:Traits::alternatives[0].info:] == nullptr);
  static_assert(Traits::alternatives[0].empty);
  static_assert(!Traits::alternatives[1].empty);
  static_assert(Traits::alternatives[1].info == ^^int);
  static_assert(!has_projection<Traits, 0, int*&>);
  static_assert(has_projection<Traits, 1, int*&>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<int*&>())), int&>);

  using VoidTraits = std::alternative_traits<void*>;
  static_assert(!has_projection<VoidTraits, 0, void*&>);
  static_assert(has_projection<VoidTraits, 1, void*&>);
  static_assert(has_alternative_info<VoidTraits, 1>);
  static_assert(VoidTraits::alternatives[1].info == ^^void);
  static_assert(std::is_void_v<decltype(VoidTraits::get<1>(std::declval<void*&>()))>);

  int value = 42;
  int* pointer = &value;
  assert(Traits::index(pointer) == 1);
  assert(Traits::get<1>(pointer) == 42);
  pointer = nullptr;
  assert(Traits::index(pointer) == 0);
  return true;
}

constexpr bool test_optional() {
  using Traits = std::alternative_traits<std::optional<int>>;
  static_assert(alternative_count<Traits> == 2);
  static_assert(!Traits::has_residual_states);
  static_assert(noexcept(Traits::index(std::declval<const std::optional<int>&>())));
  static_assert(std::is_same_v<decltype(Traits::index(std::declval<const std::optional<int>&>())), bool>);
  static_assert(!has_projection<Traits, 0, std::optional<int>&>);
  static_assert(has_projection<Traits, 1, std::optional<int>&>);
  static_assert(has_alternative_info<Traits, 0>);
  static_assert(has_alternative_info<Traits, 1>);
  static_assert(std::meta::is_object(Traits::alternatives[0].info));
  static_assert([:Traits::alternatives[0].info:] == std::nullopt);
  static_assert(Traits::alternatives[0].empty);
  static_assert(!Traits::alternatives[1].empty);
  static_assert(Traits::index(std::nullopt) == 0);
  static_assert(Traits::alternatives[1].info == ^^int);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<std::optional<int>&>())), int&>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<std::optional<int>&&>())), int&&>);

  std::optional<int> value = 42;
  assert(Traits::index(value) == 1);
  assert(Traits::get<1>(value) == 42);
  value.reset();
  assert(Traits::index(value) == 0);
  return true;
}

constexpr bool test_finite_value_traits() {
  using Partial = std::alternative_traits<std::partial_ordering>;
  static_assert(alternative_count<Partial> == 4);
  static_assert(!Partial::has_residual_states);
  static_assert(std::is_same_v<decltype(Partial::index(std::partial_ordering::less)), unsigned>);
  static_assert(Partial::index(std::partial_ordering::less) == 0);
  static_assert(Partial::index(0) == 1);
  static_assert(Partial::index(std::partial_ordering::greater) == 2);
  static_assert(Partial::index(std::partial_ordering::unordered) == 3);
  static_assert(std::meta::is_object(Partial::alternatives[0].info));
  static_assert([:Partial::alternatives[0].info:] == std::partial_ordering::less);
  static_assert([:Partial::alternatives[1].info:] == std::partial_ordering::equivalent);
  static_assert([:Partial::alternatives[2].info:] == std::partial_ordering::greater);
  static_assert([:Partial::alternatives[3].info:] == std::partial_ordering::unordered);

  using Weak = std::alternative_traits<std::weak_ordering>;
  static_assert(alternative_count<Weak> == 3);
  static_assert(!Weak::has_residual_states);
  static_assert(std::is_same_v<decltype(Weak::index(std::weak_ordering::less)), unsigned>);
  static_assert(Weak::index(std::weak_ordering::less) == 0);
  static_assert(Weak::index(0) == 1);
  static_assert(Weak::index(std::weak_ordering::greater) == 2);

  using Strong = std::alternative_traits<std::strong_ordering>;
  static_assert(alternative_count<Strong> == 3);
  static_assert(!Strong::has_residual_states);
  static_assert(std::is_same_v<decltype(Strong::index(std::strong_ordering::less)), unsigned>);
  static_assert(Strong::index(std::strong_ordering::less) == 0);
  static_assert(Strong::index(std::strong_ordering::equal) == 1);
  static_assert(Strong::index(std::strong_ordering::equivalent) == 1);
  static_assert(Strong::index(std::strong_ordering::greater) == 2);

  using Month = std::alternative_traits<std::chrono::month>;
  static_assert(alternative_count<Month> == 12);
  static_assert(Month::has_residual_states);
  static_assert(std::is_same_v<decltype(Month::index(std::chrono::January)), unsigned>);
  static_assert(Month::index(std::chrono::January) == 0);
  static_assert(Month::index(std::chrono::December) == 11);
  static_assert(Month::index(std::chrono::month{0}) >= alternative_count<Month>);
  static_assert(Month::index(std::chrono::month{13}) == 12);
  static_assert([:Month::alternatives[0].info:] == std::chrono::January);
  static_assert([:Month::alternatives[11].info:] == std::chrono::December);

  using Weekday = std::alternative_traits<std::chrono::weekday>;
  static_assert(alternative_count<Weekday> == 7);
  static_assert(Weekday::has_residual_states);
  static_assert(std::is_same_v<decltype(Weekday::index(std::chrono::Sunday)), unsigned>);
  static_assert(Weekday::index(std::chrono::Sunday) == 0);
  static_assert(Weekday::index(std::chrono::Saturday) == 6);
  static_assert(Weekday::index(std::chrono::weekday{7}) == 0);
  static_assert(Weekday::index(std::chrono::weekday{8}) == 8);
  static_assert([:Weekday::alternatives[0].info:] == std::chrono::Sunday);
  static_assert([:Weekday::alternatives[6].info:] == std::chrono::Saturday);
  return true;
}

constexpr bool test_variant() {
  using Variant = std::variant<int, double>;
  using Traits  = std::alternative_traits<Variant>;
  static_assert(alternative_count<Traits> == 2);
  static_assert(Traits::has_residual_states);
  static_assert(noexcept(Traits::index(std::declval<const Variant&>())));
  static_assert(has_projection<Traits, 0, Variant&>);
  static_assert(has_projection<Traits, 1, Variant&>);
  static_assert(Traits::alternatives[0].info == ^^int);
  static_assert(Traits::alternatives[1].info == ^^double);
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
  static_assert(alternative_count<Traits> == 2);
  static_assert(!Traits::has_residual_states);
  static_assert(noexcept(Traits::index(std::declval<const Expected&>())));
  static_assert(has_projection<Traits, 0, Expected&>);
  static_assert(has_projection<Traits, 1, Expected&>);
  static_assert(!has_projection<Traits, 2, Expected&>);
  static_assert(Traits::names::value.index == 0);
  static_assert(Traits::names::error.index == 1);
  static_assert(Traits::names::none.index == 0);
  static_assert(Traits::names::some.index == 1);
  static_assert(Traits::alternatives[0].info == ^^int);
  static_assert(Traits::alternatives[1].info == ^^long);
  static_assert(std::is_same_v<decltype(Traits::get<0>(std::declval<Expected&>())), int&>);
  static_assert(std::is_same_v<decltype(Traits::get<1>(std::declval<Expected&&>())), long&&>);
  using VoidTraits = std::alternative_traits<std::expected<void, long>>;
  static_assert(VoidTraits::alternatives[0].info == ^^void);
  static_assert(has_projection<VoidTraits, 0, std::expected<void, long>&>);
  using VoidNullableTraits = typename decltype(VoidTraits::names::some)::provider;
  static_assert(has_projection<VoidNullableTraits, 1, std::expected<void, long>&>);
  static_assert(std::is_same_v<decltype(VoidTraits::get<0>(std::declval<std::expected<void, long>&>())), void>);

  Expected value = 42;
  assert(Traits::index(value) == Traits::names::value.index);
  assert(Traits::get<0>(value) == 42);
  value = std::unexpected(7L);
  assert(Traits::index(value) == Traits::names::error.index);
  assert(Traits::get<1>(value) == 7);
  return true;
}

int main(int, char**) {
  static_assert(test_pointer());
  static_assert(test_optional());
  static_assert(test_variant());
  static_assert(test_expected());
  static_assert(test_finite_value_traits());
  return 0;
}
