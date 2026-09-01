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
#include <cassert>

int match_any(const std::any& a) {
  return a match {
    case { const int& i } if (i == 0) => 0;
    case { const int& i } => i;
    case { const double& d } => static_cast<int>(d) + 4;
    case _ => -1;
  };
}

int match_any_type_selector(const std::any& a) {
  return a match {
    case { int: auto value } => value;
    case { double: auto value } => static_cast<int>(value) + 4;
    case _ => -1;
  };
}

int match_rvalue_any(std::any&& a) {
  return static_cast<std::any&&>(a) match {
    case { int&& i } => i;
    case _ => -1;
  };
}

int match_prvalue_any() {
  return std::any(43) match {
    case { int&& i } => i;
    case _ => -1;
  };
}

int match_empty_any(const std::any& a) {
  return a match {
    case {} => 0;
    case { const int& i } => i;
    case _ => -1;
  };
}

bool test_empty_any(const std::any& a) {
  return a match case {};
}

bool test_nonempty_any(const std::any& a) {
  return a match {
    case { _ } => true;
    case {} => false;
  };
}

bool test_any_type_pattern(const std::any& a) {
  return a match {
    case { int } => true;
    case _ => false;
  };
}

int copies = 0;

struct CopyCounter {
  int value;

  CopyCounter(int value) : value(value) {}
  CopyCounter(const CopyCounter& other) : value(other.value) { ++copies; }
};

int match_any_by_value(std::any& a) {
  return a match {
    case { CopyCounter copy } if (copy.value == 0) => 0;
    case { CopyCounter copy } => copy.value;
    case _ => -1;
  };
}

int main(int, char**) {
  assert(match_any(0) == 0);
  assert(match_any(1) == 1);
  assert(match_any(2) == 2);
  assert(match_any(3.0) == 7);
  assert(match_any(4.0) == 8);
  assert(match_any(0.0f) == -1);
  assert(match_any(std::any{}) == -1);
  assert(match_any_type_selector(42) == 42);
  assert(match_any_type_selector(3.0) == 7);
  assert(match_any_type_selector(std::any{}) == -1);
  assert(match_rvalue_any(std::any(42)) == 42);
  assert(match_prvalue_any() == 43);
  assert(match_empty_any(std::any{}) == 0);
  assert(match_empty_any(std::any(42)) == 42);
  assert(test_empty_any(std::any{}));
  assert(!test_empty_any(std::any(42)));
  assert(!test_nonempty_any(std::any{}));
  assert(test_nonempty_any(std::any(42)));
  assert(test_any_type_pattern(std::any(42)));
  assert(!test_any_type_pattern(std::any(42.0)));

  std::any counted = CopyCounter(5);
  copies = 0;
  assert(match_any_by_value(counted) == 5);
  assert(copies == 2);
  return 0;
}
