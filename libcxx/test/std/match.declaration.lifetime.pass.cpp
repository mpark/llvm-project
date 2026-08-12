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
  if (case auto copy = subject)
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
  while (case auto copy = subject) {
    if (iterations++ != 0)
      break;
    if (alive != 2)
      return -1;
  }
  return alive * 10 + iterations;
}

struct TrivialMoveState {
  int value;
};

int failed_guard_preserves_trivially_moved_subject() {
  TrivialMoveState subject{7};
  return static_cast<TrivialMoveState&&>(subject) match {
    case TrivialMoveState first if (false) => first.value;
    case TrivialMoveState second if (second.value == 7) => 1;
    case _ => 2;
  };
}

const Tracked& identity(const Tracked& value) { return value; }

bool indirect_subject_lives_through_controlled_statement(int value) {
  int alive = 0;
  bool observed = false;
  if (case auto&& bound = identity(Tracked(&alive, value)))
    observed = bound.value == value && alive == 1;
  return observed && alive == 0;
}

int main(int, char**) {
  if (declaration_lifetime_in_if(0) != 2)
    return 1;
  if (declaration_lifetime_after_failed_guard() != 1)
    return 2;
  if (declaration_lifetime_in_while() != 12)
    return 3;
  if (failed_guard_preserves_trivially_moved_subject() != 1)
    return 4;
  if (!indirect_subject_lives_through_controlled_statement(1))
    return 5;
  if (!indirect_subject_lives_through_controlled_statement(2))
    return 6;
  return 0;
}
