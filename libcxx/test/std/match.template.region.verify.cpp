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
        static_assert(false, "a singleton projection is not dependent"); // expected-error {{static assertion failed: a singleton projection is not dependent}}
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
