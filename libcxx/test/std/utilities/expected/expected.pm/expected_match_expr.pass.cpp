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

static_assert(42 match { 42 => 0; _ => 1; } == 0);

constexpr int match_variant(std::variant<int, long> v) {
    return v match {
        int: _ => 0;
        long: _ => 1;
        _ => -1;
    };
}
static_assert(match_variant(std::variant<int, long>(42)) == 0);
static_assert(match_variant(std::variant<int, long>(42l)) == 1);

constexpr int match_expected(std::expected<int, long> v) {
    return v match {
        int: _ => 0;
        long: _ => 1;
    };
}
static_assert(match_expected(std::expected<int, long>(42)) == 0);
static_assert(match_expected(std::expected<int, long>(std::unexpect, 42l)) == 1);

constexpr int match_expected(std::expected<int, int> v) {
    return v match {
        int: _ => v.index();
    };
}
static_assert(match_expected(std::expected<int, int>(42)) == 0);
// TODO FIXME: This is not a constant expression for some reason....
#if 0
static_assert(match_expected(std::expected<int, int>(std::unexpect, 42)) == 1);
#endif

// TODO FIXME: This fails because the matcher `void: _` causes this error:
//
//# | /home/tzlaine/llvm-project/libcxx/test/std/utilities/expected/expected.pm/expected_pm_get.pass.cpp:64:9: error: no matching function for call to 'get'
//# |    64 |         void: _ => 0;
//# |       |         ^~~~
// [snip a lot of candidates]
//# | /home/tzlaine/llvm-project/build/runtimes/runtimes-bins/libcxx/test-suite-install/include/c++/v1/expected:111:1: note: candidate template ignored: substitution failure [with _Ip = 0, _Tp = void, _Ep = int]: cannot form a reference to 'void'
//# |   110 | _LIBCPP_AVAILABILITY_THROW_BAD_VARIANT_ACCESS constexpr variant_alternative_t<_Ip, expected<_Tp, _Ep>>&
//# |       |                                                                                                       ~
//# |   111 | get(expected<_Tp, _Ep>& __e) {
//# |       | ^
//
// This could possibly be made to work if the `T: _` case was special, and did
// not use get(), since it can see that the placeholder obviates actually
// getting a value.
#if 0
constexpr int match_expected(std::expected<void, int> v) {
    return v match {
        void: _ => 0;
        int: _ => 1;
    };
}
static_assert(match_expected(std::expected<void, int>()) == 0);
static_assert(match_expected(std::expected<void, int>(std::unexpect, 42)) == 1);
#endif

int main(int, char**) {
  return 0;
}
