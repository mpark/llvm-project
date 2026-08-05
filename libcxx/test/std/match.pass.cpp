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
#include <tuple>
#include <variant>
#include <any>

void check(bool b) { assert(b); }

void test_match_test_expr() {
  check(0 match case _);
  check(0 match case 0);
  check(!(0 match case 1));

  int x = 0;
  check(x match case _);
  check(x match case 0);

  int y = 1;
  check(!(0 match case y));
  check(!(x match case y));

  check([]() { int* p = nullptr; return p match case _; }());

  check([]() { int x = 0; return x match case 0; }());
  check(![]() { int y = 1; return 0 match case y; }());
  check([]() { int x = 0, y = 0; return x match case y; }());
  check(![]() { int x = 0, y = 1; return x match case y; }());

  check([]() { int x = 0; return &x match case { _ }; }());
  check([]() { int x = 0; return &x match case { 0 }; }());
  check(![]() { int x = 0; return &x match case { 1 }; }());
  check([]() { int x = 0, y = 0; return &x match case { y }; }());
  check(![]() { int x = 0, y = 1; return &x match case { y }; }());
  check(![]() { int* p = nullptr; return p match case { _ }; }());
  check(![]() { int* p = nullptr; return p match case { 0 }; }());

  check([]() { int x = 0, *p = &x; return &p match case { { _ } }; }());
  check([]() { int x = 0, *p = &x; return &p match case { { 0 } }; }());
  check(![]() { int x = 0, *p = &x; return &p match case { { 1 } }; }());

  check([]() { int x = 0, *p = &x; return &p match case { _ }; }());
  check([]() { int x = 0, *p = &x; return &p match case { { 0 } }; }());
  check(![]() { int x = 0, *p = &x; return &p match case { { 1 } }; }());
  check(![]() { int** pp = nullptr; return pp match case { _ }; }());
  check(![]() { int** pp = nullptr; return pp match case { { _ } }; }());
  check(![]() { int** pp = nullptr; return pp match case { { 0 } }; }());

  check([]() { return 0 match case auto&& _; }());
  check([]() { return 0 match case [[maybe_unused]] auto&& x; }());
  check([]() {
    int x = 0;
    return &x match case { [[maybe_unused]] auto&& bound };
  }());
}

auto char_pattern(char c) {
  return c match {
    case 'a'   => 1;
    case 'b'   => 2;
    case auto&& x => int(x);
  };
}

void test_char_pattern() {
  check(char_pattern('a') == 1);
  check(char_pattern('b') == 2);
  check(char_pattern('c') == 99);
}

auto decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    case [ 0, 0 ]     => -1;
    case [ auto&& x, 0 ] => x * 2;
    case [ 0, auto&& y ] => y * 4;
    case auto&& [x, y]    => x * y;
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
    case [auto&& c, [0, 0]] => {c, -1};
    case [auto&& c, [auto&& x, 0]] => {c, x * 2};
    case [auto&& c, [0, auto&& y]] => {c, y * 4};
    case [auto&& c, [auto&& x, auto&& y]] => {c, x * y};
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
      case [0, 0] => s == FizzBuzz && n == 0;
      case [0, auto&& y] => s == Fizz && n == y;
      case [auto&& x, 0] => s == Buzz && n == x;
      case auto&& [x, y] => s == N && n == x + y;
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
    case 0 => 0;
    case 1 => 3.0;
    case 2 => 'c';
    case _ => 0;
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
    case DerivedA: auto&& a => a.x * 2;
    case const DerivedB: auto&& b => (int)b.c;
    case _ => 0;
  };
}

void test_alternative_pattern_const() {
  check(alternative_pattern_const(DerivedA{101}) == 202);
  check(alternative_pattern_const(DerivedB{'a'}) == 97);
}

auto alternative_pattern_non_const(DerivedA derived) {
  Base &base = derived;
  return base match {
    case DerivedA: [auto&& x] => x * 2;
    case DerivedB: [auto&& c] => (int)c;
    case _ => 0;
  };
}

void test_alternative_pattern_non_const() {
  check(alternative_pattern_non_const(DerivedA{101}) == 202);
  check(alternative_pattern_non_const(DerivedA{202}) == 404);
}

auto bitfields(int x) {
  struct S { int i : 6; } s{x};
  return s.i match {
    case 8 => 0;
    case auto&& n => n;
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
  template <>
  struct tuple_size<Pair> {
    static constexpr int value = 2;
  };

  template <int I>
  struct tuple_element<I, Pair> {
    using type = int;
  };
}

int tuple_decomposition_pattern(const std::tuple<int, int> &tup) {
  return tup match {
    case [0, 0] => -1;
    case [0, auto&& y] => y * 2;
    case [auto&& x, 0] => x * 4;
    case auto&& [x, y] => x * y;
  };
}

int tuple_like_decomposition_pattern(const Pair &tup) {
  return tup match {
    case [0, 0] => -1;
    case [0, auto&& y] => y * 2;
    case [auto&& x, 0] => x * 4;
    case auto&& [x, y] => x * y;
  };
}

void test_tuple_like_decomposition_pattern() {
  check(tuple_decomposition_pattern({0, 0}) == -1);
  check(tuple_decomposition_pattern({0, 2}) == 4);
  check(tuple_decomposition_pattern({2, 0}) == 8);
  check(tuple_decomposition_pattern({2, 3}) == 6);
  check(tuple_like_decomposition_pattern({0, 0}) == -1);
  check(tuple_like_decomposition_pattern({0, 2}) == 4);
  check(tuple_like_decomposition_pattern({2, 0}) == 8);
  check(tuple_like_decomposition_pattern({2, 3}) == 6);
}

bool match_test_with_guard(const int (&xs)[2]) {
  return xs match case auto&& [x, y] if (x == y);
}

void test_match_test_with_guard() {
  check(match_test_with_guard({0, 0}));
  check(!match_test_with_guard({0, 1}));
  check(match_test_with_guard({1, 1}));
  check(!match_test_with_guard({2, 3}));
}

auto match_pattern_guards(const Pair& p) {
  return p match {
    case auto&& [x, y] if (x < 0 && y < 0) => 0;
    case auto&& [x, y] if (x < 0) => y;
    case auto&& [x, y] if (y < 0) => x;
    case auto&& [x, y] => x + y;
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
  if (p match case { [[maybe_unused]] auto&& v }) {
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
  Lifetime(bool* _flag, int n) : flag(_flag), n(n) { *flag = true; }
  ~Lifetime() { *flag = false; }
  bool *flag;
  int n;
};

bool match_in_if_condition_lifetime_extended(int n) {
  bool flag = false;
  if (Lifetime(&flag, n) match case [{ [[maybe_unused]] auto&& b }, 101]) {
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

bool match_in_if_condition_not_lifetime_extended(int n) {
  bool flag = false;
  if ((Lifetime(&flag, n) match case [{ _ }, 101])) {
    return flag;
  } else if (n == 202) {
    return flag;
  }
  return flag;
}

void test_match_in_if_condition_not_lifetime_extended() {
  check(!match_in_if_condition_not_lifetime_extended(101));
  check(!match_in_if_condition_not_lifetime_extended(202));
  check(!match_in_if_condition_not_lifetime_extended(303));
}

int match_in_while_condition() {
  int i = 0;
  auto next = [&]() -> int* {
    return i < 4 ? &i : nullptr;
  };
  while (next() match case { [[maybe_unused]] auto&& v }) {
    ++v;
  }
  return i;
}

void test_match_in_while_condition() {
  check(match_in_while_condition() == 4);
}

struct Variant {
  Variant(int x) : i(0), x(x) {}
  Variant(double y) : i(1), y(y) {}
  Variant(float z) : i(2), z(z) {}

  constexpr int index() const { return i; }

  template <int I>
  constexpr const auto& get() const {
    if constexpr (I == 0) return x;
    else if constexpr (I == 1) return y;
    else if constexpr (I == 2) return z;
    else static_assert(false);
  }

  int i;

  int x;
  double y;
  float z;
};

namespace std {
  template <>
  struct variant_size<Variant> {
    static constexpr int value = 3;
  };

  template <> struct variant_alternative<0, Variant> { using type = int; };
  template <> struct variant_alternative<1, Variant> { using type = double; };
  template <> struct variant_alternative<2, Variant> { using type = float; };
}

int variant_alternative_pattern(const std::variant<int, double, float> &var) {
  return var match {
    case int: 0 => 0;
    case int: 1 => 1;
    case double: auto&& y => (int)y + 4;
    case _ => -1;
  };
}

int variant_like_alternative_pattern(const Variant &var) {
  return var match {
    case int: 0 => 0;
    case int: 1 => 1;
    case double: auto&& y => (int)y + 4;
    case _ => -1;
  };
}

int classify(const int&) { return 10; }
int classify(const double&) { return 20; }
int classify(const float&) { return 30; }

int auto_alternative_pattern(const std::variant<int, double, float>& var) {
  return var match {
    case auto: auto&& value => classify(value);
  };
}

template<class T>
int dependent_auto_alternative_pattern(const T& var) {
  return var match {
    case auto: auto&& value => classify(value);
  };
}

void test_variant_like_alternative_pattern() {
  check(variant_alternative_pattern(0) == 0);
  check(variant_alternative_pattern(1) == 1);
  check(variant_alternative_pattern(2) == -1);
  check(variant_alternative_pattern(3.0) == 7);
  check(variant_alternative_pattern(4.0) == 8);
  check(variant_alternative_pattern(0.f) == -1);
  check(variant_like_alternative_pattern(0) == 0);
  check(variant_like_alternative_pattern(1) == 1);
  check(variant_like_alternative_pattern(2) == -1);
  check(variant_like_alternative_pattern(3.0) == 7);
  check(variant_like_alternative_pattern(4.0) == 8);
  check(variant_like_alternative_pattern(0.f) == -1);
  check(auto_alternative_pattern(1) == 10);
  check(auto_alternative_pattern(2.0) == 20);
  check(auto_alternative_pattern(3.0f) == 30);
  check(dependent_auto_alternative_pattern(
            std::variant<int, double, float>(1)) == 10);
  check(dependent_auto_alternative_pattern(
            std::variant<int, double, float>(2.0)) == 20);
  check(dependent_auto_alternative_pattern(
            std::variant<int, double, float>(3.0f)) == 30);
}

int match_stmt_action(int limit) {
  int r = 0;
  for (int i = limit; i >= 0; i--) {
    r += i match {
      case auto&& x if (x < 5) => 1;
      case 5 => continue;
      case 6 => break;
      case 7 => return 99;
      case _ => 0;
    };
  }
  return r;
}

void test_match_stmt_action() {
  check(match_stmt_action(5) == 5);
  check(match_stmt_action(6) == 0);
  check(match_stmt_action(7) == 99);
}

int try_cast_alternative_pattern(const std::any& a) {
  return a match {
    case int: 0 => 0;
    case int: 1 => 1;
    case double: auto&& y => (int)y + 4;
    case _ => -1;
  };
}

void test_try_cast_alternative_pattern() {
  check(try_cast_alternative_pattern(0) == 0);
  check(try_cast_alternative_pattern(1) == 1);
  check(try_cast_alternative_pattern(2) == -1);
  check(try_cast_alternative_pattern(3.0) == 7);
  check(try_cast_alternative_pattern(4.0) == 8);
  check(try_cast_alternative_pattern(0.f) == -1);
}

void test_void_returning_match() {
  0 match { case _ => []() {}(); };
}

int throw_action(int x) {
  return x match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw 101;
  };
}

void test_throw_action() {
  check(throw_action(0) == 0);
  check(throw_action(1) == 1);
  try {
    throw_action(2);
  } catch (int x) {
    check(x == 101);
  }
}

template <int... Is, int N>
int pack_expansion_in_decomposition_pattern(const int (&p)[N]) {
  return p match {
    case [0, Is...] => 0;
    case [Is..., 0] => 1;
    case _ => -1;
  };
}

void test_pack_expansion_in_decomposition_pattern() {
  check(pack_expansion_in_decomposition_pattern<1, 1>({0, 1, 1}) == 0);
  check(pack_expansion_in_decomposition_pattern<1, 1>({1, 1, 0}) == 1);
  check(pack_expansion_in_decomposition_pattern<1, 1>({0, 0, 0}) == -1);
}

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
  test_match_in_if_condition_not_lifetime_extended();
  test_match_in_while_condition();
  test_variant_like_alternative_pattern();
  test_match_stmt_action();
  test_try_cast_alternative_pattern();
  test_void_returning_match();
  test_throw_action();
  test_pack_expansion_in_decomposition_pattern();
}
