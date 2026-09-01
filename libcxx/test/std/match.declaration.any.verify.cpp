//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <any>

int naked_declaration_does_not_inspect_any(std::any& value) {
  return value match {
    case int i => i; // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'std::any'}}
    case _ => 0;
  };
}

int mutable_reference_does_not_bind_through_const_any(const std::any& value) {
  return value match {
    case { int& i } => i; // expected-error {{declaration pattern of type 'int &' is not an exact match}}
    case _ => 0;
  };
}
