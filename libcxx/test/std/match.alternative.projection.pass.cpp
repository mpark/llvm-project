//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>

int match_pointer(int* pointer) {
  return pointer match {
    case { int& value } => value;
    case {} => -1;
  };
}

int match_optional(const std::optional<int>& value) {
  return value match {
    case { const int& number } => number;
    case {} => -1;
  };
}

int match_rvalue_optional(std::optional<int>&& value) {
  return static_cast<std::optional<int>&&>(value) match {
    case { int&& number } => number;
    case {} => -1;
  };
}

int match_variant(const std::variant<int, double>& value) {
  return value match {
    case { const int& integer } => integer;
    case { const double& real } => static_cast<int>(real) + 10;
  };
}

int match_variant_type_patterns(const std::variant<int, double>& value) {
  return value match {
    case { const int& } => 12;
    case { const double& } => 13;
  };
}

int match_expected(const std::expected<int, long>& value) {
  return value match {
    case { .value: const int& result } => result;
    case { .error: const long& error } => static_cast<int>(error) + 20;
  };
}

int match_same_type_expected(const std::expected<int, int>& value) {
  return value match {
    case { .value: const int& result } => result;
    case { .error: const int& error } => error + 30;
  };
}

int match_void_expected(const std::expected<void, std::string>& value) {
  return value match {
    case { void } => 40;
    case { std::string } => 41;
  };
}

int main(int, char**) {
  int value = 42;
  assert(match_pointer(&value) == 42);
  assert(match_pointer(nullptr) == -1);
  assert(match_optional(7) == 7);
  assert(match_optional(std::nullopt) == -1);
  assert(match_rvalue_optional(std::optional<int>(8)) == 8);
  assert(match_variant(9) == 9);
  assert(match_variant(2.5) == 12);
  assert(match_variant_type_patterns(9) == 12);
  assert(match_variant_type_patterns(2.5) == 13);
  assert(match_expected(10) == 10);
  assert(match_expected(std::unexpected(3L)) == 23);
  assert(match_same_type_expected(11) == 11);
  assert(match_same_type_expected(std::unexpected(4)) == 34);
  assert(match_void_expected({}) == 40);
  assert(match_void_expected(std::unexpected(std::string("error"))) == 41);
  return 0;
}
