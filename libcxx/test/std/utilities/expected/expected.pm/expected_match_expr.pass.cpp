//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include "test_macros.h"
#include "test_workarounds.h"
#include <variant>
#include <expected>

static_assert(42 match { case 42 => 0; case _ => 1; } == 0);

constexpr int match_variant(std::variant<int, long> v) {
    return v match {
        case { int } => 0;
        case { long } => 1;
        case _ => -1;
    };
}
static_assert(match_variant(std::variant<int, long>(42)) == 0);
static_assert(match_variant(std::variant<int, long>(42l)) == 1);

constexpr int match_expected(std::expected<int, long> v) {
    return v match {
        case { int } => 0;
        case { long } => 1;
    };
}
static_assert(match_expected(std::expected<int, long>(42)) == 0);
static_assert(match_expected(std::expected<int, long>(std::unexpect, 42l)) == 1);

constexpr int match_expected(std::expected<int, int> v) {
    return v match {
        case { int } => v.index();
    };
}
static_assert(match_expected(std::expected<int, int>(42)) == 0);
// TODO FIXME: This is not a constant expression for some reason....
#if 0
static_assert(match_expected(std::expected<int, int>(std::unexpect, 42)) == 1);
#endif

constexpr int match_expected(std::expected<void, int> v) {
    return v match {
        case { void } => 0;
        case { int } => 1;
    };
}
static_assert(match_expected(std::expected<void, int>()) == 0);
static_assert(match_expected(std::expected<void, int>(std::unexpect, 42)) == 1);

int main(int, char**) {
  return 0;
}
