//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

struct Tracked {
  Tracked(int* alive, int value) : alive(alive), value(value) { ++*alive; }
  Tracked(const Tracked& other) : alive(other.alive), value(other.value) {
    ++*alive;
  }
  ~Tracked() { --*alive; }

  int* alive;
  int value;
};

int declaration_lifetime_in_if(int value) {
  int alive = 0;
  Tracked subject(&alive, value);
  if (subject match case auto copy)
    return alive;
  return -1;
}

int declaration_lifetime_after_failed_guard() {
  int alive = 0;
  Tracked subject(&alive, 0);
  if (subject match case auto copy if (false))
    return -1;
  return alive;
}

int declaration_lifetime_in_while() {
  int alive = 0;
  int iterations = 0;
  Tracked subject(&alive, 0);
  while (subject match case auto copy if (iterations++ == 0)) {
    if (alive != 2)
      return -1;
  }
  return alive * 10 + iterations;
}

int main(int, char**) {
  if (declaration_lifetime_in_if(0) != 2)
    return 1;
  if (declaration_lifetime_after_failed_guard() != 1)
    return 2;
  if (declaration_lifetime_in_while() != 12)
    return 3;
  return 0;
}
