//===----------------------------------------------------------------------===//
//
// Copyright 2024 Bloomberg Finance L.P.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03 || c++11 || c++14 || c++17 || c++20
// REQUIRES: has-fblocks
// ADDITIONAL_COMPILE_FLAGS: -fblocks
// ADDITIONAL_COMPILE_FLAGS: -freflection-latest

#include <meta>

constexpr auto block = std::meta::reflect_constant(^int() { return 4; });
static_assert(type_of(block) == ^^int(^)());

void test() {
  (void)[:block:]();
  (void)(4 ^^(void) { return 2; }());
}
