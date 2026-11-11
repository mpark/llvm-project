//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <optional>
#include <string>
#include <utility>
#include <variant>

void multiple_projections(std::variant<int, std::string> value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "discarded in each case instantiation");
    }
  }
}

void singleton_projection(std::optional<int> value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "a singleton projection is still generic");
    }
    case {} => ;
  }
}

void repeated_projections(std::variant<int, int> value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "repeated entries still introduce a region");
    }
  }
}

void named_projection(std::optional<int> value) {
  match (value) {
    case { .value: auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "a named projection is still generic");
    }
    case {} => ;
  }
}

void indexed_projection(std::variant<int, int> value) {
  match (value) {
    case { .index<0>: auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "an indexed projection is still generic");
    }
    case { .index<1>: int } => ;
  }
}

void covered_singleton_projection(std::optional<int> value) {
  match (value) {
    case { int } => ;
    case {} => ;
    case { auto payload } => static_assert(false, "not instantiated"); // expected-warning {{match case is redundant}}
  }
}

void concrete_projection(std::optional<int> value) {
  match (value) {
    case { int payload } => {
      if constexpr (true)
        (void)payload;
      else
        static_assert(false, "a concrete projection is not generic"); // expected-error {{static assertion failed: a concrete projection is not generic}}
    }
    case {} => ;
  }
}

void direct_placeholder(int value) {
  match (value) {
    case auto whole => {
      if constexpr (true)
        (void)whole;
      else
        static_assert(false, "a direct placeholder is generic");
    }
  }
}

void wrong_pair_arity(std::pair<int, int> value) {
  match (value) {
    case [auto first, auto second, auto third] => ; // expected-error {{binds to 2 elements, but 3 names were provided}}
    case _ => ;
  }
}

void wrong_pair_binding_arity(std::pair<int, int> value) {
  match (value) {
    case auto [first, second, third] => ; // expected-error {{binds to 2 elements, but 3 names were provided}}
    case _ => ;
  }
}

void covered_direct_placeholder(int value) {
  match (value) {
    case int => ;
    case auto whole => whole.size(); // expected-warning {{match case is redundant}}
  }
}

void non_dependent_handler_error(int value) {
  match (value) {
    case auto whole => no_such_name; // expected-error {{use of undeclared identifier 'no_such_name'}}
  }
}
