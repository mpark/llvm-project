//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <meta>

int variable;
void function();

consteval int classify(std::meta::info reflection) {
  return match (reflection) {
    case std::meta::is_type => 0;
    case std::meta::is_variable => 1;
    case auto([](std::meta::info candidate) consteval {
      return std::meta::is_function(candidate);
    }) => 2;
    case _ => 3;
  };
}

static_assert(classify(^^int) == 0);
static_assert(classify(^^variable) == 1);
static_assert(classify(^^function) == 2);
static_assert(classify(^^::) == 3);

int main(int, char**) {}
