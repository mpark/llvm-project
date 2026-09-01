//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <chrono>
#include <compare>
#include <optional>
#include <string>
#include <variant>

template <class V>
int dependent_missing_alternative(V value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: { std::string }}}
    case { int number } => number;
  };
}

int instantiate_dependent_missing_alternative() {
  return dependent_missing_alternative(std::variant<int, std::string>(1)); // expected-note {{in instantiation of function template specialization 'dependent_missing_alternative<std::variant<int, std::string>>' requested here}}
}

template <class V>
int dependent_maybe_useful_alternative(V value) {
  return value match {
    case { int } => 0;
    case { std::string } => 1;
    case { char character } => static_cast<int>(character);
  };
}

int instantiate_dependent_maybe_useful_alternative() {
  return dependent_maybe_useful_alternative(std::variant<int, std::string>(1));
}

int missing_partial_ordering(std::partial_ordering value) {
  return value match { // expected-error {{match expression is not exhaustive}}
    case std::partial_ordering::less => -1;
    case std::partial_ordering::equivalent => 0;
    case std::partial_ordering::greater => 1;
  };
}

int duplicate_strong_ordering_alias(std::strong_ordering value) {
  return value match {
    case std::strong_ordering::less => -1;
    case std::strong_ordering::equal => 0;
    case std::strong_ordering::equivalent => 0; // expected-error {{match case is redundant}}
    case std::strong_ordering::greater => 1;
  };
}

int duplicate_weekday_alias(std::chrono::weekday value) {
  return value match {
    case std::chrono::Sunday => 0;
    case std::chrono::weekday{7} => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

int duplicate_optional_empty_state(std::optional<int> value) {
  return value match {
    case std::nullopt => 0;
    case {} => 1; // expected-error {{match case is redundant}}
    case { int number } => number;
  };
}

int missing_month(std::chrono::month value) {
  return value match { // expected-error {{match expression is not exhaustive}}
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
  };
}
