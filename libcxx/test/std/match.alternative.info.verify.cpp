//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <variant>

constexpr std::alternative_info typed_empty{^^int, /*empty=*/true};
// expected-error@-1 {{constexpr variable 'typed_empty' must be initialized by a constant expression}}
