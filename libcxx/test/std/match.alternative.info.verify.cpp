//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <expected>
#include <variant>

constexpr std::alternative_info typed_empty{^^int, /*empty=*/true};
// expected-error@-1 {{constexpr variable 'typed_empty' must be initialized by a constant expression}}

int expected_has_no_null_name(const std::expected<int, long>& value) {
  return match (value) {
    case { .null } => 0; // expected-error {{alternative name 'null' is not defined}}
    case _ => 1;
  };
}
