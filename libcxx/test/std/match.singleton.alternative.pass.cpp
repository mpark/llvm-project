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
#include <optional>
#include <variant>

constexpr int match_optional(const std::optional<int>& value) {
  return value match {
    case std::nullopt => -1;
    case { const int& number } => number;
  };
}

constexpr int match_optional_zero(const std::optional<int>& value) {
  return value match {
    case 0 => 0;
    case {} => 1;
    case { const int& } => 2;
  };
}

constexpr int match_pointer(const int* pointer) {
  return pointer match {
    case nullptr => -1;
    case { const int& number } => number;
  };
}

constexpr int match_partial_ordering(std::partial_ordering value) {
  return value match {
    case std::partial_ordering::less => -1;
    case std::partial_ordering::equivalent => 0;
    case std::partial_ordering::greater => 1;
    case std::partial_ordering::unordered => 2;
  };
}

constexpr int match_strong_ordering(std::strong_ordering value) {
  return value match {
    case std::strong_ordering::less => -1;
    case 0 => 0;
    case std::strong_ordering::greater => 1;
  };
}

constexpr unsigned match_month(std::chrono::month value) {
  return value match {
    case std::chrono::January => 1;
    case std::chrono::February => 2;
    case std::chrono::March => 3;
    case std::chrono::April => 4;
    case std::chrono::May => 5;
    case std::chrono::June => 6;
    case std::chrono::July => 7;
    case std::chrono::August => 8;
    case std::chrono::September => 9;
    case std::chrono::October => 10;
    case std::chrono::November => 11;
    case std::chrono::December => 12;
    case _ => 0;
  };
}

constexpr unsigned match_valid_month(std::chrono::month value) {
  return value match {
    case std::chrono::January => 1;
    case std::chrono::February => 2;
    case std::chrono::March => 3;
    case std::chrono::April => 4;
    case std::chrono::May => 5;
    case std::chrono::June => 6;
    case std::chrono::July => 7;
    case std::chrono::August => 8;
    case std::chrono::September => 9;
    case std::chrono::October => 10;
    case std::chrono::November => 11;
    case std::chrono::December => 12;
  };
}

constexpr unsigned match_weekday(std::chrono::weekday value) {
  return value match {
    case std::chrono::Sunday => 0;
    case std::chrono::Monday => 1;
    case std::chrono::Tuesday => 2;
    case std::chrono::Wednesday => 3;
    case std::chrono::Thursday => 4;
    case std::chrono::Friday => 5;
    case std::chrono::Saturday => 6;
    case _ => 7;
  };
}

constexpr unsigned match_valid_weekday(std::chrono::weekday value) {
  return value match {
    case std::chrono::Sunday => 0;
    case std::chrono::Monday => 1;
    case std::chrono::Tuesday => 2;
    case std::chrono::Wednesday => 3;
    case std::chrono::Thursday => 4;
    case std::chrono::Friday => 5;
    case std::chrono::Saturday => 6;
  };
}

constexpr int match_nested(
    const std::variant<std::partial_ordering, int>& value) {
  return value match {
    case { std::partial_ordering::less } => -1;
    case { std::partial_ordering::equivalent } => 0;
    case { std::partial_ordering::greater } => 1;
    case { std::partial_ordering::unordered } => 2;
    case { const int& number } => number;
  };
}

constexpr bool test() {
  std::optional<int> optional;
  if (match_optional(optional) != -1)
    return false;
  optional = 42;
  if (match_optional(optional) != 42)
    return false;
  if (match_optional_zero(std::nullopt) != 1 ||
      match_optional_zero(std::optional<int>(0)) != 0 ||
      match_optional_zero(std::optional<int>(1)) != 2)
    return false;

  int number = 7;
  if (match_pointer(nullptr) != -1 || match_pointer(&number) != 7)
    return false;

  if (match_partial_ordering(std::partial_ordering::unordered) != 2 ||
      match_strong_ordering(std::strong_ordering::equal) != 0)
    return false;

  if (match_month(std::chrono::April) != 4 ||
      match_month(std::chrono::month{13}) != 0 ||
      match_valid_month(std::chrono::December) != 12)
    return false;
  if (match_weekday(std::chrono::Friday) != 5 ||
      match_weekday(std::chrono::weekday{8}) != 7 ||
      match_valid_weekday(std::chrono::Saturday) != 6)
    return false;

  if (match_nested(std::partial_ordering::greater) != 1 ||
      match_nested(42) != 42)
    return false;
  return true;
}

int main(int, char**) {
  static_assert(test());
  assert(test());
  return 0;
}
