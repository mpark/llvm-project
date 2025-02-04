//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>

void check(bool b) { assert(b); }

void test_match_test_expr() {
  check(0 match _);
  check(0 match 0);
  check(!(0 match 1));

  int x = 0;
  check(x match _);
  check(x match 0);

  int y = 1;
  check(!(0 match y));
  check(!(x match y));

  check([]() { int* p = nullptr; return p match _; }());

  check([]() { int x = 0; return x match 0; }());
  check(![]() { int y = 1; return 0 match y; }());
  check([]() { int x = 0, y = 0; return x match y; }());
  check(![]() { int x = 0, y = 1; return x match y; }());

  check([]() { int x = 0; return &x match ? _; }());
  check([]() { int x = 0; return &x match ? 0; }());
  check(![]() { int x = 0; return &x match ? 1; }());
  check([]() { int x = 0, y = 0; return &x match ? y; }());
  check(![]() { int x = 0, y = 1; return &x match ? y; }());
  check(![]() { int* p = nullptr; return p match ? _; }());
  check(![]() { int* p = nullptr; return p match ? 0; }());

  check([]() { int x = 0, *p = &x; return &p match ?? _; }());
  check([]() { int x = 0, *p = &x; return &p match ?? 0; }());
  check(![]() { int x = 0, *p = &x; return &p match ?? 1; }());

  check([]() { int x = 0, *p = &x; return &p match ? _; }());
  check([]() { int x = 0, *p = &x; return &p match ?? 0; }());
  check(![]() { int x = 0, *p = &x; return &p match ?? 1; }());
  check(![]() { int** pp = nullptr; return pp match ? _; }());
  check(![]() { int** pp = nullptr; return pp match ?? _; }());
  check(![]() { int** pp = nullptr; return pp match ?? 0; }());

  check([]() { return 0 match let _; }());
  check([]() { return 0 match let x; }());
  check([]() {
    int x = 0;
    return &x match ? let x;
  }());
}

auto char_pattern(char c) {
  return c match {
    'a'   => 1;
    'b'   => 2;
    let x => int(x);
  };
}

void test_char_pattern() {
  check(char_pattern('a') == 1);
  check(char_pattern('b') == 2);
  check(char_pattern('c') == 99);
}

auto decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    [ 0, 0 ]     => -1;
    [ let x, 0 ] => x * 2;
    [ 0, let y ] => y * 4;
    let[x, y]    => x * y;
  };
}

void test_decomposition_pattern() {
  check(decomposition_pattern({0, 0}) == -1);
  check(decomposition_pattern({0, 0}) != 0);
  check(decomposition_pattern({1, 0}) == 2);
  check(decomposition_pattern({1, 0}) != 3);
  check(decomposition_pattern({2, 0}) == 4);
  check(decomposition_pattern({0, 1}) == 4);
  check(decomposition_pattern({0, 2}) == 8);
  check(decomposition_pattern({2, 3}) == 6);
  check(decomposition_pattern({3, 4}) == 12);
}

enum Color { Red, Blue };

struct S {
  Color color;
  int xs[2];
};

struct Result {
  Color color;
  int i;
  bool operator==(const Result&) const noexcept = default;
};

auto nested_decomposition_pattern(const S& s) {
  return s match -> Result {
    [let c, [0, 0]] => {c, -1};
    [let c, [let x, 0]] => {c, x * 2};
    [let c, [0, let y]] => {c, y * 4};
    let [c, [x, y]] => {c, x * y};
  };
}

void test_nested_decomposition_pattern() {
  check(nested_decomposition_pattern({Red, {0, 0}}) == Result{Red, -1});
  check(nested_decomposition_pattern({Red, {0, 0}}) == Result{Red, -1});
  check(nested_decomposition_pattern({Red, {0, 0}}) != Result{Red, 0});
  check(nested_decomposition_pattern({Red, {1, 0}}) == Result{Red, 2});
  check(nested_decomposition_pattern({Red, {1, 0}}) != Result{Red, 3});
  check(nested_decomposition_pattern({Red, {2, 0}}) == Result{Red, 4});
  check(nested_decomposition_pattern({Blue, {0, 1}}) == Result{Blue, 4});
  check(nested_decomposition_pattern({Blue, {0, 2}}) == Result{Blue, 8});
  check(nested_decomposition_pattern({Blue, {2, 3}}) == Result{Blue, 6});
  check(nested_decomposition_pattern({Blue, {3, 4}}) == Result{Blue, 12});
}

enum State { FizzBuzz, Fizz, Buzz, N };
constexpr int Size = 15;

bool fizzbuzz(const State (&states)[Size], const int (&elems)[Size]) {
  bool result = true;
  for (int i = 1; i <= Size; ++i) {
    State s = states[i - 1];
    int n = elems[i - 1];
    result &= (int[2]){i % 3, i % 5} match {
      [0, 0] => s == FizzBuzz && n == 0;
      [0, let y] => s == Fizz && n == y;
      [let x, 0] => s == Buzz && n == x;
      let [x, y] => s == N && n == x + y;
    };
  }
  return result;
}

void test_fizzbuzz() {
  check(fizzbuzz(
    {N, N, Fizz, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, FizzBuzz},
    {2, 4, 3,    5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0       }
  ));

  check(!fizzbuzz(
    {N, N, Fizz, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, Fizz},
    {2, 4, 3,    5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0   }
  ));
}

auto trailing_return_type(int x) {
  return x match -> int {
    0 => 0;
    1 => 3.0;
    2 => 'c';
  };
}

void test_trailing_return_type() {
  check(trailing_return_type(0) == 0);
  check(trailing_return_type(1) == 3);
  check(trailing_return_type(2) == 99);
}

struct Base { virtual ~Base() = default; };

struct DerivedA : Base {
  int x;
  DerivedA(int x) : x(x) {}
};

struct DerivedB : Base {
  char c;
  DerivedB(char c) : c(c) {}
};

auto alternative_pattern_const(const Base &base) {
  return base match {
    DerivedA: let a => a.x * 2;
    const DerivedB: let b => (int)b.c;
    _ => 0;
  };
}

void test_alternative_pattern_const() {
  check(alternative_pattern_const(DerivedA{101}) == 202);
  check(alternative_pattern_const(DerivedB{'a'}) == 97);
}

auto alternative_pattern_non_const(DerivedA derived) {
  Base &base = derived;
  return base match {
    DerivedA: [let x] => x * 2;
    DerivedB: [let c] => (int)c;
    _ => 0;
  };
}

void test_alternative_pattern_non_const() {
  check(alternative_pattern_non_const(DerivedA{101}) == 202);
  check(alternative_pattern_non_const(DerivedA{202}) == 404);
}

auto bitfields(int x) {
  struct S { int i : 6; } s{x};
  return s.i match {
    8 => 0;
    let n => n;
  };
}

void test_bitfields() {
  check(bitfields(8) == 0);
  check(bitfields(2) == 2);
  check(bitfields(4) == 4);
}

struct Pair {
  template <int I>
  constexpr auto&& get(this auto&& self) {
    if constexpr (I == 0) return decltype(self)(self).x;
    else if constexpr (I == 1) return decltype(self)(self).y;
    else static_assert(false);
  }

  int x;
  int y;
};

namespace std {
  template <typename T>
  struct tuple_size;

  template <typename T>
  requires requires { tuple_size<T>::value; }
  struct tuple_size<const T> {
    static constexpr int value = std::tuple_size<T>::value;
  };

  template <>
  struct tuple_size<Pair> {
    static constexpr int value = 2;
  };

  template <int I, typename T>
  struct tuple_element;

  template <int I, class T>
  struct tuple_element<I, const T> {
    using type = typename std::tuple_element<I, T>::type const;
  };

  template <int I>
  struct tuple_element<I, Pair> {
    using type = int;
  };
}

int tuple_like_decomposition_pattern(const Pair &tup) {
  return tup match {
    [0, 0] => -1;
    [0, let y] => y * 2;
    [let x, 0] => x * 4;
    let [x, y] => x * y;
    _ => 0;
  };
}

void test_tuple_like_decomposition_pattern() {
  check(tuple_like_decomposition_pattern({0, 0}) == -1);
  check(tuple_like_decomposition_pattern({0, 2}) == 4);
  check(tuple_like_decomposition_pattern({2, 0}) == 8);
  check(tuple_like_decomposition_pattern({2, 3}) == 6);
}

bool match_test_with_guard(const int (&xs)[2]) {
  return xs match let [x, y] if (x == y);
}

void test_match_test_with_guard() {
  check(match_test_with_guard({0, 0}));
  check(!match_test_with_guard({0, 1}));
  check(match_test_with_guard({1, 1}));
  check(!match_test_with_guard({2, 3}));
}

auto match_pattern_guards(const Pair& p) {
  return p match {
    let [x, y] if (x < 0 && y < 0) => 0;
    let [x, y] if (x < 0) => y;
    let [x, y] if (y < 0) => x;
    let [x, y] => x + y;
  };
}

void test_match_pattern_guards() {
  check(match_pattern_guards({-1, -2}) == 0);
  check(match_pattern_guards({0, 0}) == 0);
  check(match_pattern_guards({-1, 2}) == 2);
  check(match_pattern_guards({3, 0}) == 3);
  check(match_pattern_guards({4, 7}) == 11);
}

int match_in_if_condition(const int *p) {
  if (p match ? let v) {
    return v;
  }
  return -1;
}

void test_match_in_if_condition() {
  check(match_in_if_condition(nullptr) == -1);
  int x = 0;
  check(match_in_if_condition(&x) == 0);
  int y = 1;
  check(match_in_if_condition(&y) == 1);
}

struct Lifetime {
  Lifetime(bool *flag, int n) : flag(flag), n(n) { *flag = true; }
  ~Lifetime() { *flag = false; }
  bool *flag;
  int n;
};

bool match_in_if_condition_lifetime_extended(int n) {
  bool flag = false;
  if (Lifetime(&flag, n) match [? let b, 101]) {
    return b;
  } else if (n == 202) {
    return flag;
  }
  return flag;
}

void test_match_in_if_condition_lifetime_extended() {
  check(match_in_if_condition_lifetime_extended(101));
  check(match_in_if_condition_lifetime_extended(202));
  check(!match_in_if_condition_lifetime_extended(303));
}

// bool match_in_if_condition_not_lifetime_extended(int n) {
//   bool flag = false;
//   if ((Lifetime(&flag, n) match [? let b, 101])) {
//     return flag;
//   } else if (n == 202) {
//     return flag;
//   }
//   return flag;
// }

// void test_match_in_if_condition_not_lifetime_extended() {
//   check(!match_in_if_condition_not_lifetime_extended(101));
//   check(!match_in_if_condition_not_lifetime_extended(202));
//   check(!match_in_if_condition_not_lifetime_extended(303));
// }

int match_in_while_condition() {
  int i = 0;
  auto next = [&]() -> int* {
    return i < 4 ? &i : nullptr;
  };
  while (next() match ? let v) {
    ++v;
  }
  return i;
}

void test_match_in_while_condition() {
  check(match_in_while_condition() == 4);
}

// struct Variant {
//   Variant(int x) : i(0), x(x) {}
//   Variant(double y) : i(1), y(y) {}
//   Variant(float z) : i(2), z(z) {}

//   constexpr int index() const { return i; }

//   template <int I>
//   constexpr const auto& get() const {
//     if constexpr (I == 0) return x;
//     else if constexpr (I == 1) return y;
//     else if constexpr (I == 2) return z;
//     else static_assert(false);
//   }

//   int i;

//   int x;
//   double y;
//   float z;
// };

// namespace std {
//   template <typename T>
//   struct variant_size;

//   template <typename T>
//   struct variant_size<const T> {
//     static constexpr int value = std::variant_size<T>::value;
//   };

//   template <>
//   struct variant_size<Variant> {
//     static constexpr int value = 3;
//   };

//   template <int I, typename T>
//   struct variant_alternative;

//   template <int I, class T>
//   struct variant_alternative<I, const T> {
//     using type = typename std::variant_alternative<I, T>::type const;
//   };

//   template <> struct variant_alternative<0, Variant> { using type = int; };
//   template <> struct variant_alternative<1, Variant> { using type = double; };
//   template <> struct variant_alternative<2, Variant> { using type = float; };
// }

// int variant_like_alternative_pattern(const Variant &var) {
//   return var match {
//     int: 0 => 0;
//     int: 1 => 1;
//     double: let y => (int)y + 4;
//     _ => -1;
//   };
// }

// void test_variant_like_alternative_pattern() {
//   check(variant_like_alternative_pattern(0) == 0);
//   check(variant_like_alternative_pattern(1) == 1);
//   check(variant_like_alternative_pattern(2) == -1);
//   check(variant_like_alternative_pattern(3.0) == 7);
//   check(variant_like_alternative_pattern(4.0) == 8);
//   check(variant_like_alternative_pattern(0.f) == -1);
// }

int main() {
  test_match_test_expr();
  test_char_pattern();
  test_decomposition_pattern();
  test_nested_decomposition_pattern();
  test_fizzbuzz();
  test_trailing_return_type();
  test_alternative_pattern_const();
  test_alternative_pattern_non_const();
  test_bitfields();
  test_tuple_like_decomposition_pattern();
  test_match_test_with_guard();
  test_match_pattern_guards();
  test_match_in_if_condition();
  test_match_in_if_condition_lifetime_extended();
  // test_match_in_if_condition_not_lifetime_extended();
  test_match_in_while_condition();
  // test_variant_like_alternative_pattern();
}
