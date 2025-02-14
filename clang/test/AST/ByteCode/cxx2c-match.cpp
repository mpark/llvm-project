// RUN: %clang_cc1 -std=c++2c -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value %s

void test_decltypes() {
  constexpr int x = 0;
  constexpr int y = 0;
  static_assert(__is_same(decltype(0 match _), bool));
  static_assert(__is_same(decltype(x match 0), bool));
  static_assert(__is_same(decltype(x match y), bool));
  static_assert(__is_same(decltype(&x match ? _), bool));
  static_assert(__is_same(decltype(&x match ? 0), bool));
}

static_assert(0 match _);
static_assert(0 match 0);
static_assert(!(0 match 1));

constexpr int x = 0;
static_assert(x match _);
static_assert(x match 0);

constexpr int y = 1;
static_assert(!(0 match y));
static_assert(!(x match y));

static_assert([]() { return 0 match let _; }());
static_assert([]() { return 0 match let x; }());
static_assert([]() { int x = 0; return &x match ? let x; }());

static_assert([](int x) { return x match _; }(0));
static_assert([](auto x) -> bool { return x match _; }(0));
static_assert([](int* p) { return p match _; }(nullptr));
static_assert([](auto* p) -> bool { return p match _; }((int*)nullptr));

static_assert([](int x) { return x match 0; }(0));
static_assert([](auto x) -> bool { return x match 0; }(0));
static_assert(![](int y) { return 0 match y; }(1));
static_assert(![](auto y) -> bool { return 0 match y; }(1));
static_assert([](int x, int y) { return x match y; }(0, 0));
static_assert([](auto x, auto y) -> bool { return x match y; }(0, 0));
static_assert(![](int x, int y) { return x match y; }(0, 1));
static_assert(![](auto x, auto y) -> bool { return x match y; }(0, 1));

static_assert([](int x) { return &x match ? _; }(0));
static_assert([](auto x) { return &x match ? _; }(0));
static_assert([](int x) { return &x match ? 0; }(0));
static_assert([](auto x) { return &x match ? 0; }(0));
static_assert(![](int x) { return &x match ? 1; }(0));
static_assert(![](auto x) { return &x match ? 1; }(0));
static_assert([](int x, int y) { return &x match ? y; }(0, 0));
static_assert([](auto x, auto y) { return &x match ? y; }(0, 0));
static_assert(![](int x, int y) { return &x match ? y; }(0, 1));
static_assert(![](auto x, auto y) { return &x match ? y; }(0, 1));
static_assert(![](int *p) { return p match ? _; }(nullptr));
static_assert(![](auto *p) { return p match ? _; }((int*)nullptr));
static_assert(![](int *p) { return p match ? 0; }(nullptr));
static_assert(![](auto *p) { return p match ? 0; }((int*)nullptr));

static_assert([](int x) { int *p = &x; return &p match ?? _; }(0));
static_assert([](auto x) { auto *p = &x; return &p match ?? _; }(0));
static_assert([](int x) { int *p = &x; return &p match ?? 0; }(0));
static_assert([](auto x) { auto *p = &x; return &p match ?? 0; }(0));
static_assert(![](int x) { int *p = &x; return &p match ?? 1; }(0));
static_assert(![](auto x) { auto *p = &x; return &p match ?? 1; }(0));

static_assert([](int x) { int *p = &x; return &p match ? _; }(0));
static_assert([](auto x) { auto *p = &x; return &p match ? _; }(0));
static_assert([](int x) { int *p = &x; return &p match ?? 0; }(0));
static_assert([](auto x) { auto *p = &x; return &p match ?? 0; }(0));
static_assert(![](int x) { int *p = &x; return &p match ?? 1; }(0));
static_assert(![](auto x) { auto *p = &x; return &p match ?? 1; }(0));
static_assert(![](int **pp) { return pp match ? _; }(nullptr));
static_assert(![](auto **pp) { return pp match ? _; }((int**)nullptr));
static_assert(![](int **pp) { return pp match ?? _; }(nullptr));
static_assert(![](auto **pp) { return pp match ?? _; }((int**)nullptr));
static_assert(![](int **pp) { return pp match ?? 0; }(nullptr));
static_assert(![](auto **pp) { return pp match ?? 0; }((int**)nullptr));

constexpr bool test_dependent_match_0(auto x, auto y) { return x match y; }

static_assert(test_dependent_match_0(0, 0));
static_assert(test_dependent_match_0(0.0, 0));
static_assert(!test_dependent_match_0(1, 0));

constexpr bool test_dependent_match_1(auto x) { return x match 0; }

static_assert(test_dependent_match_1(0));
static_assert(test_dependent_match_1(0.0));
static_assert(!test_dependent_match_1(1));

constexpr bool test_dependent_match_2(auto y) { return 0 match y; }

static_assert(test_dependent_match_2(0));
static_assert(test_dependent_match_2(0.0));
static_assert(!test_dependent_match_2(1));

constexpr auto test(char c) {
  return c match {
    'a' => 1;
    'b' => 2;
    let x => int(x);
  };
}

static_assert(test('a') == 1);
static_assert(test('b') == 2);
static_assert(test('c') == 99);

template <auto v>
constexpr auto test_dependent(auto c) {
  return c match {
    'a' => 1;
    v => 2;
    let x => int(x);
  };
}

static_assert(test_dependent<'b'>('a') == 1);
static_assert(test_dependent<'b'>('b') == 2);
static_assert(test_dependent<'b'>('c') == 99);

constexpr auto test_decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    [0, 0] => -1;
    [let x, 0] => x * 2;
    [0, let y] => y * 4;
    let [x, y] => x * y;
  };
}

static_assert(test_decomposition_pattern({0, 0}) == -1);
static_assert(test_decomposition_pattern({0, 0}) != 0);
static_assert(test_decomposition_pattern({1, 0}) == 2);
static_assert(test_decomposition_pattern({1, 0}) != 3);
static_assert(test_decomposition_pattern({2, 0}) == 4);
static_assert(test_decomposition_pattern({0, 1}) == 4);
static_assert(test_decomposition_pattern({0, 2}) == 8);
static_assert(test_decomposition_pattern({2, 3}) == 6);
static_assert(test_decomposition_pattern({3, 4}) == 12);

enum Color { Red, Blue };

struct S {
  Color color;
  int xs[2];
};

struct Result {
  Color color;
  int i;
  constexpr bool operator==(const Result&) const noexcept = default;
};

constexpr auto test_nested_decomposition_pattern(const S& s) {
  return s match -> Result {
    [let c, [0, 0]] => {c, -1};
    [let c, [let x, 0]] => {c, x * 2};
    [let c, [0, let y]] => {c, y * 4};
    let [c, [x, y]] => {c, x * y};
  };
}

static_assert(test_nested_decomposition_pattern({Red, {0, 0}}) == Result{Red, -1});
static_assert(test_nested_decomposition_pattern({Red, {0, 0}}) == Result{Red, -1});
static_assert(test_nested_decomposition_pattern({Red, {0, 0}}) != Result{Red, 0});
static_assert(test_nested_decomposition_pattern({Red, {1, 0}}) == Result{Red, 2});
static_assert(test_nested_decomposition_pattern({Red, {1, 0}}) != Result{Red, 3});
static_assert(test_nested_decomposition_pattern({Red, {2, 0}}) == Result{Red, 4});
static_assert(test_nested_decomposition_pattern({Blue, {0, 1}}) == Result{Blue, 4});
static_assert(test_nested_decomposition_pattern({Blue, {0, 2}}) == Result{Blue, 8});
static_assert(test_nested_decomposition_pattern({Blue, {2, 3}}) == Result{Blue, 6});
static_assert(test_nested_decomposition_pattern({Blue, {3, 4}}) == Result{Blue, 12});

enum State { FizzBuzz, Fizz, Buzz, N };
constexpr int Size = 15;

constexpr bool fizzbuzz(const State (&states)[Size], const int (&elems)[Size]) {
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

static_assert(fizzbuzz(
  {N, N, Fizz, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, FizzBuzz},
  {2, 4, 3,    5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0       }
));

static_assert(!fizzbuzz(
  {N, N, Fizz, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, Fizz},
  {2, 4, 3,    5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0   }
));

constexpr auto test_trailing_return_type(int x) {
  return x match -> int {
    0 => 0;
    1 => 3.0;
    2 => 'c';
  };
}

static_assert(test_trailing_return_type(0) == 0);
static_assert(test_trailing_return_type(1) == 3);
static_assert(test_trailing_return_type(2) == 99);

struct Base { virtual ~Base() = default; };

struct DerivedA : Base {
  int x;
  constexpr DerivedA(int x) : x(x) {}
};

struct DerivedB : Base {
  char c;
  constexpr DerivedB(char c) : c(c) {}
};

constexpr auto test_alternative_pattern_const(const Base &base) {
  return base match {
    DerivedA: let a => ({
      static_assert(__is_same(decltype(a), const DerivedA));
      static_assert(__is_same(decltype((a)), const DerivedA &));
      static_assert(__is_same(decltype(a.x), int));
      static_assert(__is_same(decltype((a.x)), const int&));
      a.x * 2;
    });
    const DerivedB: let b => ({
      static_assert(__is_same(decltype(b), const DerivedB));
      static_assert(__is_same(decltype((b)), const DerivedB &));
      static_assert(__is_same(decltype(b.c), char));
      static_assert(__is_same(decltype((b.c)), const char&));
      (int)b.c;
    });
    _ => 0;
  };
}

static_assert(test_alternative_pattern_const(DerivedA{101}) == 202);
static_assert(test_alternative_pattern_const(DerivedB{'a'}) == 97);

constexpr auto test_alternative_pattern_non_const(DerivedA derived) {
  Base &base = derived;
  return base match {
    DerivedA: [let x] => ({
      static_assert(__is_same(decltype(x), int));
      static_assert(__is_same(decltype((x)), int&));
      x * 2;
    });
    DerivedB: [let c] => ({
      static_assert(__is_same(decltype(c), char));
      static_assert(__is_same(decltype((c)), char&));
      (int)c;
    });
    _ => 0;
  };
}

static_assert(test_alternative_pattern_non_const(DerivedA{101}) == 202);
static_assert(test_alternative_pattern_non_const(DerivedA{202}) == 404);

constexpr auto test_bitfields(int x) {
  struct S { int i : 6; } s{x};
  return s.i match {
    8 => 0;
    let n => n;
  };
}

static_assert(test_bitfields(8) == 0);
static_assert(test_bitfields(2) == 2);
static_assert(test_bitfields(4) == 4);

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

constexpr int test_tuple_like_decomposition_pattern(const Pair &tup) {
  return tup match {
    [0, 0] => -1;
    [0, let y] => y * 2;
    [let x, 0] => x * 4;
    let [x, y] => x * y;
    _ => 0;
  };
}

static_assert(test_tuple_like_decomposition_pattern({0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern({0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern({2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern({2, 3}) == 6);

constexpr int test_tuple_like_decomposition_pattern_dependent(const auto &tup) {
  return tup match {
    [0, 0] => -1;
    [0, let y] => y * 2;
    [let x, 0] => x * 4;
    let [x, y] => x * y;
    _ => 0;
  };
}

static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 3}) == 6);

constexpr bool test_match_test_with_guard(const int (&xs)[2]) {
  return xs match let [x, y] if (x == y);
}

static_assert(test_match_test_with_guard({0, 0}));
static_assert(!test_match_test_with_guard({0, 1}));
static_assert(test_match_test_with_guard({1, 1}));
static_assert(!test_match_test_with_guard({2, 3}));

constexpr auto test_match_pattern_guards(const Pair& p) {
  return p match {
    let [x, y] if (x < 0 && y < 0) => 0;
    let [x, y] if (x < 0) => y;
    let [x, y] if (y < 0) => x;
    let [x, y] => x + y;
  };
}

static_assert(test_match_pattern_guards({-1, -2}) == 0);
static_assert(test_match_pattern_guards({0, 0}) == 0);
static_assert(test_match_pattern_guards({-1, 2}) == 2);
static_assert(test_match_pattern_guards({3, 0}) == 3);
static_assert(test_match_pattern_guards({4, 7}) == 11);

constexpr int test_match_in_if_condition(const int *p) {
  if (p match ? let v) {
    return v;
  }
  return -1;
}

static_assert(test_match_in_if_condition(nullptr) == -1);
static_assert(test_match_in_if_condition(&x) == 0);
static_assert(test_match_in_if_condition(&y) == 1);

struct Lifetime {
  constexpr Lifetime(bool *flag, int n) : flag(flag), n(n) { *flag = true; }
  constexpr ~Lifetime() { *flag = false; }
  bool *flag;
  int n;
};

constexpr bool test_match_in_if_condition_lifetime_extended(int n) {
  bool flag = false;
  if (Lifetime(&flag, n) match [? let b, 101]) {
    return b;
  } else if (n == 202) {
    return flag;
  }
  return flag;
}

static_assert(test_match_in_if_condition_lifetime_extended(101));
static_assert(test_match_in_if_condition_lifetime_extended(202));
static_assert(!test_match_in_if_condition_lifetime_extended(303));

constexpr bool test_match_in_if_condition_not_lifetime_extended(int n) {
  bool flag = false;
  if ((Lifetime(&flag, n) match [? let b, 101])) {
    return flag;
  } else if (n == 202) {
    return flag;
  }
  return flag;
}

static_assert(!test_match_in_if_condition_not_lifetime_extended(101));
static_assert(!test_match_in_if_condition_not_lifetime_extended(202));
static_assert(!test_match_in_if_condition_not_lifetime_extended(303));

constexpr int test_match_in_while_condition() {
  int i = 0;
  auto next = [&]() -> int* {
    return i < 4 ? &i : nullptr;
  };
  while (next() match ? let v) {
    ++v;
  }
  return i;
}

static_assert(test_match_in_while_condition() == 4);

struct Variant {
  constexpr Variant(int x) : i(0), x(x) {}
  constexpr Variant(double y) : i(1), y(y) {}
  constexpr Variant(float z) : i(2), z(z) {}

  constexpr int index() const { return i; }

  template <int I>
  constexpr const auto& get() const {
    if constexpr (I == 0) {
      return x;
    } else if constexpr (I == 1) {
      return y;
    } else if constexpr (I == 2) {
      return z;
    }
  }

  int i;

  int x;
  double y;
  float z;
};

namespace std {
  template <typename T>
  struct variant_size;

  template <typename T>
  requires requires { variant_size<T>::value; }
  struct variant_size<const T> {
    static constexpr int value = std::variant_size<T>::value;
  };

  template <>
  struct variant_size<Variant> {
    static constexpr int value = 3;
  };

  template <int I, typename T>
  struct variant_alternative;

  template <int I, class T>
  struct variant_alternative<I, const T> {
    using type = typename std::variant_alternative<I, T>::type const;
  };

  template <> struct variant_alternative<0, Variant> { using type = int; };
  template <> struct variant_alternative<1, Variant> { using type = double; };
  template <> struct variant_alternative<2, Variant> { using type = float; };
}

constexpr int test_variant_like_alternative_pattern(const Variant &var) {
  return var match {
    int: 0 => 0;
    int: 1 => 1;
    double: let y => (int)y + 4;
    _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern(0) == 0);
static_assert(test_variant_like_alternative_pattern(1) == 1);
static_assert(test_variant_like_alternative_pattern(2) == -1);
static_assert(test_variant_like_alternative_pattern(3.0) == 7);
static_assert(test_variant_like_alternative_pattern(4.0) == 8);
static_assert(test_variant_like_alternative_pattern(0.f) == -1);

template <typename T, typename U>
constexpr int test_variant_like_alternative_pattern_dependent(const auto &var) {
  return var match {
    T: 0 => 0;
    T: 1 => 1;
    U: let y => (int)y + 4;
    _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(0)) == 0);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(1)) == 1);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(2)) == -1);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(3.0)) == 7);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(4.0)) == 8);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(0.f)) == -1);

namespace N1 {
  struct S {
    int index;
    int i;
    double d;
  };

  template <typename T>
  constexpr const T* try_cast(const S& s) {
    if constexpr (__is_same(T, int)) {
      return s.index == 0 ? &s.i : nullptr;
    } else if constexpr (__is_same(T, double)) {
      return s.index == 1 ? &s.d : nullptr;
    } else {
      return nullptr;
    }
  }
}

constexpr int test_try_cast_alternative_pattern(const N1::S& s) {
  return s match -> int {
    int: let x => x;
    double: let d => d;
    short: let s => s;
    _ => -1;
  };
}

static_assert(test_try_cast_alternative_pattern(N1::S{0, 1, 2.2}) == 1);
static_assert(test_try_cast_alternative_pattern(N1::S{1, 1, 2.2}) == 2);
static_assert(test_try_cast_alternative_pattern(N1::S{2, 1, 2.2}) == -1);

template <typename T>
concept integral = __is_integral(T);

template <typename T, typename U>
concept same = __is_same(T, U);

constexpr int test_variant_like_alternative_pattern_with_type_constraint(const Variant &var) {
  return var match {
    integral: 0 => 0;
    same<int>: 1 => 1;
    double: let y => (int)y + 4;
    _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern_with_type_constraint(0) == 0);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(1) == 1);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(2) == -1);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(3.0) == 7);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(4.0) == 8);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(0.f) == -1);
