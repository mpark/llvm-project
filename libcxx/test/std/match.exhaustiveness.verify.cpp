//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

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
