//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// REQUIRES: clang

// RUN: %{cxx} %s %{flags} %{compile_flags} -fpattern-matching -O0 -stdlib=libc++ -S -emit-llvm -o %t.ll
// RUN: ! grep "get_if" %t.ll
// RUN: ! grep "variantIJidEE5index" %t.ll

#include <variant>

using Variant = std::variant<int, double>;
using Traits  = std::alternative_traits<Variant>;

int& project_int(Variant& value) {
  return Traits::get<0>(value);
}
