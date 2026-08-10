//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23
// REQUIRES: clang

// RUN: %{cxx} %s %{flags} %{compile_flags} -O0 -stdlib=libc++ -S -emit-llvm -o %t.ll
// RUN: ! grep "expectedIilE5value" %t.ll
// RUN: ! grep "bad_expected_access" %t.ll

#include <expected>

using Expected = std::expected<int, long>;
using Traits   = std::alternative_traits<Expected>;

int& project_value(Expected& value) {
  return Traits::get<0>(value);
}
