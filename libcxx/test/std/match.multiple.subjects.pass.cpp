//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>

struct MoveOnly {
  int value;
  int* alive;
  int* moves;

  MoveOnly(int value, int* alive, int* moves)
      : value(value), alive(alive), moves(moves) {
    ++*alive;
  }
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&& other)
      : value(other.value), alive(other.alive), moves(other.moves) {
    ++*alive;
    ++*moves;
    other.value = -1;
  }
  ~MoveOnly() { --*alive; }
};

int main(int, char**) {
  int first = 1;
  int second = 2;
  match (first, second) {
    case [int& x, int& y] => {
      x = 3;
      y = 4;
    }
  }
  assert(first == 3 && second == 4);
  assert((match(first, second, case [3, 4])));
  assert(!(match(first, second, case [4, 3])));

  int alive = 0;
  int moves = 0;
  int result = match (MoveOnly(5, &alive, &moves),
                      MoveOnly(7, &alive, &moves)) {
    case [MoveOnly x, MoveOnly y] =>
        (assert(alive == 4), assert(moves == 2), x.value + y.value);
  };
  assert(result == 12);
  assert(alive == 0);

  int test_alive = 0;
  int test_moves = 0;
  bool matched =
      match(MoveOnly(8, &test_alive, &test_moves),
             MoveOnly(9, &test_alive, &test_moves),
             case [MoveOnly x, MoveOnly y]);
  assert(matched);
  assert(test_moves == 2);
  assert(test_alive == 0);

  return 0;
}
