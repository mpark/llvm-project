// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -Wno-unused-variable -Wno-unused-value %s

void test_decltypes() {
  constexpr int x = 0;
  constexpr int y = 0;
  static_assert(__is_same(decltype(0 match case _), bool));
  static_assert(__is_same(decltype(x match case 0), bool));
  static_assert(__is_same(decltype(x match case y), bool));
  static_assert(__is_same(decltype(&x match case ? _), bool));
  static_assert(__is_same(decltype(&x match case ? 0), bool));
}

static_assert(0 match case _);
static_assert(0 match case 0);
static_assert(!(0 match case 1));

constexpr int x = 0;
static_assert(x match case _);
static_assert(x match case 0);

constexpr int y = 1;
static_assert(!(0 match case y));
static_assert(!(x match case y));

static_assert([]() { return 0 match case let _; }());
static_assert([]() { return 0 match case let x; }());
static_assert([]() { int x = 0; return &x match case ? let x; }());

static_assert([](int x) { return x match case _; }(0));
static_assert([](auto x) -> bool { return x match case _; }(0));
static_assert([](int* p) { return p match case _; }(nullptr));
static_assert([](auto* p) -> bool { return p match case _; }((int*)nullptr));

static_assert([](int x) { return x match case 0; }(0));
static_assert([](auto x) -> bool { return x match case 0; }(0));
static_assert(![](int y) { return 0 match case y; }(1));
static_assert(![](auto y) -> bool { return 0 match case y; }(1));
static_assert([](int x, int y) { return x match case y; }(0, 0));
static_assert([](auto x, auto y) -> bool { return x match case y; }(0, 0));
static_assert(![](int x, int y) { return x match case y; }(0, 1));
static_assert(![](auto x, auto y) -> bool { return x match case y; }(0, 1));

static_assert([](int x) { return &x match case ? _; }(0));
static_assert([](auto x) { return &x match case ? _; }(0));
static_assert([](int x) { return &x match case ? 0; }(0));
static_assert([](auto x) { return &x match case ? 0; }(0));
static_assert(![](int x) { return &x match case ? 1; }(0));
static_assert(![](auto x) { return &x match case ? 1; }(0));
static_assert([](int x, int y) { return &x match case ? y; }(0, 0));
static_assert([](auto x, auto y) { return &x match case ? y; }(0, 0));
static_assert(![](int x, int y) { return &x match case ? y; }(0, 1));
static_assert(![](auto x, auto y) { return &x match case ? y; }(0, 1));
static_assert(![](int *p) { return p match case ? _; }(nullptr));
static_assert(![](auto *p) { return p match case ? _; }((int*)nullptr));
static_assert(![](int *p) { return p match case ? 0; }(nullptr));
static_assert(![](auto *p) { return p match case ? 0; }((int*)nullptr));

static_assert([](int x) { int *p = &x; return &p match case ?? _; }(0));
static_assert([](auto x) { auto *p = &x; return &p match case ?? _; }(0));
static_assert([](int x) { int *p = &x; return &p match case ?? 0; }(0));
static_assert([](auto x) { auto *p = &x; return &p match case ?? 0; }(0));
static_assert(![](int x) { int *p = &x; return &p match case ?? 1; }(0));
static_assert(![](auto x) { auto *p = &x; return &p match case ?? 1; }(0));

static_assert([](int x) { int *p = &x; return &p match case ? _; }(0));
static_assert([](auto x) { auto *p = &x; return &p match case ? _; }(0));
static_assert([](int x) { int *p = &x; return &p match case ?? 0; }(0));
static_assert([](auto x) { auto *p = &x; return &p match case ?? 0; }(0));
static_assert(![](int x) { int *p = &x; return &p match case ?? 1; }(0));
static_assert(![](auto x) { auto *p = &x; return &p match case ?? 1; }(0));
static_assert(![](int **pp) { return pp match case ? _; }(nullptr));
static_assert(![](auto **pp) { return pp match case ? _; }((int**)nullptr));
static_assert(![](int **pp) { return pp match case ?? _; }(nullptr));
static_assert(![](auto **pp) { return pp match case ?? _; }((int**)nullptr));
static_assert(![](int **pp) { return pp match case ?? 0; }(nullptr));
static_assert(![](auto **pp) { return pp match case ?? 0; }((int**)nullptr));

constexpr bool test_dependent_match_0(auto x, auto y) { return x match case y; }

static_assert(test_dependent_match_0(0, 0));
static_assert(test_dependent_match_0(0.0, 0));
static_assert(!test_dependent_match_0(1, 0));

constexpr bool test_dependent_match_1(auto x) { return x match case 0; }

static_assert(test_dependent_match_1(0));
static_assert(test_dependent_match_1(0.0));
static_assert(!test_dependent_match_1(1));

constexpr bool test_dependent_match_2(auto y) { return 0 match case y; }

static_assert(test_dependent_match_2(0));
static_assert(test_dependent_match_2(0.0));
static_assert(!test_dependent_match_2(1));

constexpr auto test(char c) {
  return c match {
    case 'a' => 1;
    case 'b' => 2;
    case let x => int(x);
  };
}

static_assert(test('a') == 1);
static_assert(test('b') == 2);
static_assert(test('c') == 99);

template <auto v>
constexpr auto test_dependent(auto c) {
  return c match {
    case 'a' => 1;
    case v => 2;
    case let x => int(x);
  };
}

static_assert(test_dependent<'b'>('a') == 1);
static_assert(test_dependent<'b'>('b') == 2);
static_assert(test_dependent<'b'>('c') == 99);

constexpr auto test_decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    case [0, 0] => -1;
    case [let x, 0] => x * 2;
    case [0, let y] => y * 4;
    case let [x, y] => x * y;
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

constexpr int test_vector_decomposition_pattern() {
  using FourUInts = unsigned __attribute__((__vector_size__(16)));
  FourUInts four_uints = {1, 2, 3, 4};
  return four_uints match {
    case [1, 2, 3, 4] => 10;
    case _ => 20;
  };
}

static_assert(test_vector_decomposition_pattern() == 10);

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
    case [let c, [0, 0]] => {c, -1};
    case [let c, [let x, 0]] => {c, x * 2};
    case [let c, [0, let y]] => {c, y * 4};
    case let [c, [x, y]] => {c, x * y};
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
      case [0, 0] => s == FizzBuzz && n == 0;
      case [0, let y] => s == Fizz && n == y;
      case [let x, 0] => s == Buzz && n == x;
      case let [x, y] => s == N && n == x + y;
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
    case 0 => 0;
    case 1 => 3.0;
    case 2 => 'c';
    case _ => 0;
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
    case DerivedA: let a => ({
      static_assert(__is_same(decltype(a), const DerivedA));
      static_assert(__is_same(decltype((a)), const DerivedA &));
      static_assert(__is_same(decltype(a.x), int));
      static_assert(__is_same(decltype((a.x)), const int&));
      a.x * 2;
    });
    case const DerivedB: let b => ({
      static_assert(__is_same(decltype(b), const DerivedB));
      static_assert(__is_same(decltype((b)), const DerivedB &));
      static_assert(__is_same(decltype(b.c), char));
      static_assert(__is_same(decltype((b.c)), const char&));
      (int)b.c;
    });
    case _ => 0;
  };
}

static_assert(test_alternative_pattern_const(DerivedA{101}) == 202);
static_assert(test_alternative_pattern_const(DerivedB{'a'}) == 97);

constexpr auto test_alternative_pattern_non_const(DerivedA derived) {
  Base &base = derived;
  return base match {
    case DerivedA: [let x] => ({
      static_assert(__is_same(decltype(x), int));
      static_assert(__is_same(decltype((x)), int&));
      x * 2;
    });
    case DerivedB: [let c] => ({
      static_assert(__is_same(decltype(c), char));
      static_assert(__is_same(decltype((c)), char&));
      (int)c;
    });
    case _ => 0;
  };
}

static_assert(test_alternative_pattern_non_const(DerivedA{101}) == 202);
static_assert(test_alternative_pattern_non_const(DerivedA{202}) == 404);

constexpr auto test_bitfields(int x) {
  struct S { int i : 6; } s{x};
  return s.i match {
    case 8 => 0;
    case let n => n;
  };
}

static_assert(test_bitfields(8) == 0);
static_assert(test_bitfields(2) == 2);
static_assert(test_bitfields(4) == 4);

constexpr int test_bitfield_decomposition(unsigned x, unsigned y) {
  struct S { unsigned opc : 16, imm : 16; } s{x, y};
  return s match {
    case [let opc, let imm] => int(opc + imm);
  };
}

static_assert(test_bitfield_decomposition(1, 2) == 3);
static_assert(test_bitfield_decomposition(5, 7) == 12);

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
    case [0, 0] => -1;
    case [0, let y] => y * 2;
    case [let x, 0] => x * 4;
    case let [x, y] => x * y;
  };
}

static_assert(test_tuple_like_decomposition_pattern({0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern({0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern({2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern({2, 3}) == 6);

constexpr int test_tuple_like_decomposition_pattern_dependent(const auto &tup) {
  return tup match {
    case [0, 0] => -1;
    case [0, let y] => y * 2;
    case [let x, 0] => x * 4;
    case let [x, y] => x * y;
    case _ => 0;
  };
}

static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 3}) == 6);

constexpr bool test_match_test_with_guard(const int (&xs)[2]) {
  return xs match case let [x, y] if (x == y);
}

static_assert(test_match_test_with_guard({0, 0}));
static_assert(!test_match_test_with_guard({0, 1}));
static_assert(test_match_test_with_guard({1, 1}));
static_assert(!test_match_test_with_guard({2, 3}));

constexpr auto test_match_pattern_guards(const Pair& p) {
  return p match {
    case let [x, y] if (x < 0 && y < 0) => 0;
    case let [x, y] if (x < 0) => y;
    case let [x, y] if (y < 0) => x;
    case let [x, y] => x + y;
  };
}

static_assert(test_match_pattern_guards({-1, -2}) == 0);
static_assert(test_match_pattern_guards({0, 0}) == 0);
static_assert(test_match_pattern_guards({-1, 2}) == 2);
static_assert(test_match_pattern_guards({3, 0}) == 3);
static_assert(test_match_pattern_guards({4, 7}) == 11);

constexpr int test_match_in_if_condition(const int *p) {
  if (p match case ? let v) {
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
  if (Lifetime(&flag, n) match case [? let b, 101]) {
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
  if ((Lifetime(&flag, n) match case [? let b, 101])) {
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
  while (next() match case ? let v) {
    ++v;
  }
  return i;
}

static_assert(test_match_in_while_condition() == 4);

constexpr int test_match_guard_init_statement(const Pair &p) {
  return p match {
    case let [x, y] if (int sum = x + y; sum < 0) => sum;
    case let [x, y] => x + y;
  };
}

static_assert(test_match_guard_init_statement({-1, -2}) == -3);
static_assert(test_match_guard_init_statement({4, 7}) == 11);

constexpr int test_match_test_guard_init_statement(int value) {
  if (value match case let copy if (int doubled = copy * 2; doubled == 4))
    return doubled;
  return -1;
}

static_assert(test_match_test_guard_init_statement(2) == 4);
static_assert(test_match_test_guard_init_statement(3) == -1);

struct GuardInitLifetime {
  int &live;

  constexpr GuardInitLifetime(int &live) : live(live) { ++live; }
  constexpr ~GuardInitLifetime() { --live; }
};

constexpr int test_match_guard_init_lifetime(bool take_first) {
  int live = 0;
  int result = take_first match {
    case _ if (GuardInitLifetime guard(live); take_first) => live;
    case _ => live;
  };
  return result * 10 + live;
}

static_assert(test_match_guard_init_lifetime(true) == 10);
static_assert(test_match_guard_init_lifetime(false) == 0);

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
    case int: 0 => 0;
    case int: 1 => 1;
    case double: let y => (int)y + 4;
    case _ => -1;
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
    case T: 0 => 0;
    case T: 1 => 1;
    case U: let y => (int)y + 4;
    case _ => -1;
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
    case int: let x => x;
    case double: let d => d;
    case short: let s => s;
    case _ => -1;
  };
}

static_assert(test_try_cast_alternative_pattern(N1::S{0, 1, 2.2}) == 1);
static_assert(test_try_cast_alternative_pattern(N1::S{1, 1, 2.2}) == 2);
static_assert(test_try_cast_alternative_pattern(N1::S{2, 1, 2.2}) == -1);

template <typename T>
concept integral = __is_integral(T);

template <typename T, typename U>
concept same = __is_same(T, U);

template <typename T>
concept arithmetic =
    __is_same(T, int) || __is_same(T, double) || __is_same(T, float);

template <typename T>
struct alternative_code;

template <typename T>
struct alternative_code<const T> : alternative_code<T> {};

template <>
struct alternative_code<int> {
  static constexpr int value = 1;
};

template <>
struct alternative_code<double> {
  static constexpr int value = 2;
};

template <>
struct alternative_code<float> {
  static constexpr int value = 3;
};

constexpr int test_variant_like_alternative_pattern_with_type_constraint(const Variant &var) {
  return var match {
    case integral: 0 => 0;
    case same<int>: 1 => 1;
    case double: let y => (int)y + 4;
    case _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern_with_type_constraint(0) == 0);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(1) == 1);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(2) == -1);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(3.0) == 7);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(4.0) == 8);
static_assert(test_variant_like_alternative_pattern_with_type_constraint(0.f) == -1);

constexpr int test_concept_selects_every_matching_alternative(const Variant &var) {
  return var match {
    case arithmetic: let value =>
        alternative_code<decltype(value)>::value * 10 +
        static_cast<int>(value);
  };
}

static_assert(test_concept_selects_every_matching_alternative(1) == 11);
static_assert(test_concept_selects_every_matching_alternative(2.0) == 22);
static_assert(test_concept_selects_every_matching_alternative(3.0f) == 33);

constexpr int test_auto_selects_every_alternative(const Variant &var) {
  return var match {
    case auto: let value => alternative_code<decltype(value)>::value;
  };
}

static_assert(test_auto_selects_every_alternative(1) == 1);
static_assert(test_auto_selects_every_alternative(2.0) == 2);
static_assert(test_auto_selects_every_alternative(3.0f) == 3);

template <int... Is, int N>
constexpr int test_pack_expansion_in_decomposition_pattern(const int (&p)[N]) {
  return p match {
    case [0, Is...] => 0;
    case [Is..., 0] => 1;
    case _ => -1;
  };
}

static_assert(test_pack_expansion_in_decomposition_pattern<1, 1>({0, 1, 1}) == 0);
static_assert(test_pack_expansion_in_decomposition_pattern<1, 1>({1, 1, 0}) == 1);
static_assert(test_pack_expansion_in_decomposition_pattern<1, 1>({0, 0, 0}) == -1);

constexpr int match_subject_is_evaluated_once() {
  int evaluations = 0;
  return (++evaluations, 5) match {
    case let value if (value < 0) => 0;
    case let value => evaluations;
  };
}

static_assert(match_subject_is_evaluated_once() == 1);

struct ConstantMatchSubject {
  int value;
};

constexpr bool constant_prvalue_match_subject_has_one_identity() {
  const ConstantMatchSubject *saved = nullptr;
  return ConstantMatchSubject{42} match {
    case let value if ((saved = &value, false)) => false;
    case let value => &value == saved;
  };
}

static_assert(constant_prvalue_match_subject_has_one_identity());

struct SharedMatchProjection {
  int first;
  int second;
  int *projections;

  template <int I>
  constexpr int &get() & {
    static_assert(I == 0 || I == 1);
    ++projections[I];
    if constexpr (I == 0)
      return first;
    else
      return second;
  }
};

namespace std {
template <>
struct tuple_size<SharedMatchProjection> {
  static constexpr int value = 2;
};

template <int I>
struct tuple_element<I, SharedMatchProjection> {
  static_assert(I == 0 || I == 1);
  using type = int;
};
} // namespace std

constexpr int structural_arms_share_match_projections() {
  int projections[2] = {};
  SharedMatchProjection source{1, 2, projections};
  int result = source match {
    case [0, 0] => 0;
    case [let x, 0] => x;
    case [0, let y] => y;
    case [let x, let y] => x + y;
  };
  return projections[0] * 100 + projections[1] * 10 + result;
}

static_assert(structural_arms_share_match_projections() == 113);

constexpr int dependent_structural_arms_share_match_projections(auto &source) {
  return source match {
    case [0, 0] => 0;
    case [1, 0] => 1;
    case [0, 2] => 2;
    case [_, _] => 3;
  };
}

constexpr int instantiate_shared_match_projection() {
  int projections[2] = {};
  SharedMatchProjection source{1, 2, projections};
  int result = dependent_structural_arms_share_match_projections(source);
  return projections[0] * 100 + projections[1] * 10 + result;
}

static_assert(instantiate_shared_match_projection() == 113);

constexpr bool reference_match_result_selects_referent(bool first) {
  int x = 1;
  int y = 2;
  int &selected = first match -> int & {
    case true => static_cast<int &>(x);
    case false => static_cast<int &>(y);
  };
  selected = 3;
  return x == (first ? 3 : 1) && y == (first ? 2 : 3);
}

static_assert(reference_match_result_selects_referent(true));
static_assert(reference_match_result_selects_referent(false));
