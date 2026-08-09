// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -Wno-unused-variable -Wno-unused-value %s

void test_decltypes() {
  constexpr int x = 0;
  constexpr int y = 0;
  static_assert(__is_same(decltype(0 match case _), bool));
  static_assert(__is_same(decltype(x match case 0), bool));
  static_assert(__is_same(decltype(x match case y), bool));
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

static_assert([]() { return 0 match case auto&& _; }());
static_assert([]() { return 0 match case auto&& x; }());

static_assert([](int x) { return x match case _; }(0));
static_assert([](auto x) -> bool { return x match case _; }(0));
static_assert([](int* p) { return p match case _; }(nullptr));
static_assert([](auto* p) -> bool { return p match case _; }((int*)nullptr));
static_assert([](int* p) { return p match case {}; }(nullptr));
static_assert(![](int value) { return &value match case {}; }(0));

constexpr int match_pointer(int *pointer) {
  return pointer match {
    case {} => -1;
    case { auto &&value } => value;
  };
}

static_assert(match_pointer(nullptr) == -1);
static_assert([] { int value = 42; return match_pointer(&value); }() == 42);

static_assert([](int x) { return x match case 0; }(0));
static_assert([](auto x) -> bool { return x match case 0; }(0));
static_assert(![](int y) { return 0 match case y; }(1));
static_assert(![](auto y) -> bool { return 0 match case y; }(1));
static_assert([](int x, int y) { return x match case y; }(0, 0));
static_assert([](auto x, auto y) -> bool { return x match case y; }(0, 0));
static_assert(![](int x, int y) { return x match case y; }(0, 1));
static_assert(![](auto x, auto y) -> bool { return x match case y; }(0, 1));

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
    case auto&& x => int(x);
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
    case auto&& x => int(x);
  };
}

static_assert(test_dependent<'b'>('a') == 1);
static_assert(test_dependent<'b'>('b') == 2);
static_assert(test_dependent<'b'>('c') == 99);

constexpr auto test_decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    case [0, 0] => -1;
    case [auto&& x, 0] => x * 2;
    case [0, auto&& y] => y * 4;
    case auto&& [x, y] => x * y;
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
    case [auto&& c, [0, 0]] => {c, -1};
    case [auto&& c, [auto&& x, 0]] => {c, x * 2};
    case [auto&& c, [0, auto&& y]] => {c, y * 4};
    case [auto&& c, [auto&& x, auto&& y]] => {c, x * y};
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
      case [0, auto&& y] => s == Fizz && n == y;
      case [auto&& x, 0] => s == Buzz && n == x;
      case auto&& [x, y] => s == N && n == x + y;
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
    case const DerivedA& a => ({
      static_assert(__is_same(decltype(a), const DerivedA&));
      static_assert(__is_same(decltype((a)), const DerivedA &));
      static_assert(__is_same(decltype(a.x), int));
      static_assert(__is_same(decltype((a.x)), const int&));
      a.x * 2;
    });
    case const DerivedB& b => ({
      static_assert(__is_same(decltype(b), const DerivedB&));
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
    case DerivedA& a => ({
      static_assert(__is_same(decltype(a), DerivedA&));
      static_assert(__is_same(decltype((a)), DerivedA&));
      a.x * 2;
    });
    case DerivedB& b => ({
      static_assert(__is_same(decltype(b), DerivedB&));
      static_assert(__is_same(decltype((b)), DerivedB&));
      (int)b.c;
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
    case auto&& n => n;
  };
}

static_assert(test_bitfields(8) == 0);
static_assert(test_bitfields(2) == 2);
static_assert(test_bitfields(4) == 4);

constexpr int test_bitfield_decomposition(unsigned x, unsigned y) {
  struct S { unsigned opc : 16, imm : 16; } s{x, y};
  return s match {
    case [auto opc, auto imm] => int(opc + imm);
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
    case [0, auto&& y] => y * 2;
    case [auto&& x, 0] => x * 4;
    case auto&& [x, y] => x * y;
  };
}

static_assert(test_tuple_like_decomposition_pattern({0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern({0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern({2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern({2, 3}) == 6);

constexpr int test_tuple_like_decomposition_pattern_dependent(const auto &tup) {
  return tup match {
    case [0, 0] => -1;
    case [0, auto&& y] => y * 2;
    case [auto&& x, 0] => x * 4;
    case auto&& [x, y] => x * y;
    case _ => 0;
  };
}

static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 0}) == -1);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{0, 2}) == 4);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 0}) == 8);
static_assert(test_tuple_like_decomposition_pattern_dependent(Pair{2, 3}) == 6);

constexpr bool test_match_test_with_guard(const int (&xs)[2]) {
  return xs match case auto&& [x, y] if (x == y);
}

static_assert(test_match_test_with_guard({0, 0}));
static_assert(!test_match_test_with_guard({0, 1}));
static_assert(test_match_test_with_guard({1, 1}));
static_assert(!test_match_test_with_guard({2, 3}));

constexpr auto test_match_pattern_guards(const Pair& p) {
  return p match {
    case auto&& [x, y] if (x < 0 && y < 0) => 0;
    case auto&& [x, y] if (x < 0) => y;
    case auto&& [x, y] if (y < 0) => x;
    case auto&& [x, y] => x + y;
  };
}

static_assert(test_match_pattern_guards({-1, -2}) == 0);
static_assert(test_match_pattern_guards({0, 0}) == 0);
static_assert(test_match_pattern_guards({-1, 2}) == 2);
static_assert(test_match_pattern_guards({3, 0}) == 3);
static_assert(test_match_pattern_guards({4, 7}) == 11);

constexpr int test_match_guard_init_statement(const Pair &p) {
  return p match {
    case auto&& [x, y] if (int sum = x + y; sum < 0) => sum;
    case auto&& [x, y] => x + y;
  };
}

static_assert(test_match_guard_init_statement({-1, -2}) == -3);
static_assert(test_match_guard_init_statement({4, 7}) == 11);

constexpr int test_match_test_guard_init_statement(int value) {
  if (value match case int copy if (int doubled = copy * 2; doubled == 4))
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
  constexpr Variant(int x, int &index_calls, int &get_calls)
      : i(0), x(x), index_calls(&index_calls), get_calls(&get_calls) {}

  constexpr int index() const {
    if (index_calls)
      ++*index_calls;
    return i;
  }

  template <int I>
  constexpr auto& get() {
    if (get_calls)
      ++*get_calls;
    if constexpr (I == 0) {
      return x;
    } else if constexpr (I == 1) {
      return y;
    } else if constexpr (I == 2) {
      return z;
    }
  }

  template <int I>
  constexpr const auto& get() const {
    if (get_calls)
      ++*get_calls;
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
  int *index_calls = nullptr;
  int *get_calls = nullptr;
};

namespace std {
  template <typename T>
  struct alternative_traits;

  template <>
  struct alternative_traits<Variant> {
    static constexpr __SIZE_TYPE__ size = 3;

    template <__SIZE_TYPE__ I>
    using projection_type = __type_pack_element<I, int, double, float>;

    static constexpr __SIZE_TYPE__ index(const Variant& value) noexcept {
      return value.index();
    }

    template <__SIZE_TYPE__ I, class Self>
    static constexpr decltype(auto) get(Self&& value) {
      return static_cast<Self&&>(value).template get<I>();
    }
  };
}

constexpr int test_variant_like_alternative_pattern(const Variant &var) {
  return var match {
    case { int integer } => integer match {
      case 0 => 0;
      case 1 => 1;
      case _ => -1;
    };
    case { double real } => (int)real + 4;
    case _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern(0) == 0);
static_assert(test_variant_like_alternative_pattern(1) == 1);
static_assert(test_variant_like_alternative_pattern(2) == -1);
static_assert(test_variant_like_alternative_pattern(3.0) == 7);
static_assert(test_variant_like_alternative_pattern(4.0) == 8);
static_assert(test_variant_like_alternative_pattern(0.f) == -1);

constexpr int variant_arms_share_projection() {
  int index_calls = 0;
  int get_calls = 0;
  Variant var(1, index_calls, get_calls);
  int result = var match {
    case { int integer } => integer match {
      case 0 => 0;
      case _ => integer;
    };
    case _ => -1;
  };
  return index_calls * 100 + get_calls * 10 + result;
}

static_assert(variant_arms_share_projection() == 111);

template <typename T, typename U>
constexpr int test_variant_like_alternative_pattern_dependent(const auto &var) {
  return var match {
    case { T first } => first match {
      case 0 => 0;
      case 1 => 1;
      case _ => -1;
    };
    case { U second } => (int)second + 4;
    case _ => -1;
  };
}

static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(0)) == 0);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(1)) == 1);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(2)) == -1);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(3.0)) == 7);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(4.0)) == 8);
static_assert(test_variant_like_alternative_pattern_dependent<int, double>(Variant(0.f)) == -1);

constexpr int dependent_variant_arms_share_projection() {
  int index_calls = 0;
  int get_calls = 0;
  Variant var(1, index_calls, get_calls);
  int result =
      test_variant_like_alternative_pattern_dependent<int, double>(var);
  return index_calls * 100 + get_calls * 10 + result;
}

static_assert(dependent_variant_arms_share_projection() == 111);

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

template <>
struct std::alternative_traits<N1::S> {
  template <typename T, typename Self>
  static constexpr auto try_cast(Self&& value) {
    return N1::try_cast<T>(static_cast<Self&&>(value));
  }
};

constexpr int test_try_cast_declaration_pattern(const N1::S& s) {
  return s match -> int {
    case { const int& i } if (i == 0) => 0;
    case { const int& i } => i;
    case { const double& d } => d;
    case { const short& value } => value;
    case _ => -1;
  };
}

static_assert(test_try_cast_declaration_pattern(N1::S{0, 1, 2.2}) == 1);
static_assert(test_try_cast_declaration_pattern(N1::S{1, 1, 2.2}) == 2);
static_assert(test_try_cast_declaration_pattern(N1::S{2, 1, 2.2}) == -1);

template <typename T>
concept arithmetic =
    __is_same(T, int) || __is_same(T, double) || __is_same(T, float);

template <typename T>
struct alternative_code;

template <typename T>
struct alternative_code<const T> : alternative_code<T> {};

template <typename T>
struct alternative_code<T&> : alternative_code<T> {};

template <typename T>
struct alternative_code<T&&> : alternative_code<T> {};

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

constexpr int test_concept_selects_every_matching_alternative(const Variant &var) {
  return var match {
    case { arithmetic auto value } =>
        alternative_code<decltype(value)>::value * 10 +
        static_cast<int>(value);
  };
}

static_assert(test_concept_selects_every_matching_alternative(1) == 11);
static_assert(test_concept_selects_every_matching_alternative(2.0) == 22);
static_assert(test_concept_selects_every_matching_alternative(3.0f) == 33);

constexpr int test_auto_selects_every_alternative(const Variant &var) {
  return var match {
    case { auto&& value } => alternative_code<decltype(value)>::value;
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

struct GuardProjection {
  int value;
  int *projections;

  template<int I>
  constexpr int get() && {
    static_assert(I == 0);
    ++*projections;
    return value;
  }

  template<int I>
  constexpr int &get() & {
    static_assert(I == 0);
    ++*projections;
    return value;
  }
};

struct SharedProjection {
  int first;
  int second;
  int *projections;

  template<int I>
  constexpr int &get() & {
    static_assert(I == 0 || I == 1);
    ++projections[I];
    if constexpr (I == 0)
      return first;
    else
      return second;
  }
};

struct SharedVariantProjection {
  SharedProjection value;
  int *index_calls;
  int *projection_calls;

  constexpr int index() const {
    ++*index_calls;
    return 0;
  }

  template<int I>
  constexpr SharedProjection &get() {
    static_assert(I == 0);
    ++*projection_calls;
    return value;
  }
};

namespace std {
template<>
struct tuple_size<GuardProjection> {
  static constexpr int value = 1;
};

template<int I>
struct tuple_element<I, GuardProjection> {
  static_assert(I == 0);
  using type = int;
};

template<>
struct tuple_size<SharedProjection> {
  static constexpr int value = 2;
};

template<int I>
struct tuple_element<I, SharedProjection> {
  static_assert(I == 0 || I == 1);
  using type = int;
};

template<>
struct alternative_traits<SharedVariantProjection> {
  static constexpr __SIZE_TYPE__ size = 1;

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  using projection_type = SharedProjection;

  static constexpr __SIZE_TYPE__
  index(const SharedVariantProjection& value) noexcept {
    return value.index();
  }

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  static constexpr SharedProjection& get(SharedVariantProjection& value) {
    return value.template get<I>();
  }
};
} // namespace std

namespace declaration_patterns {

struct Pair {
  int first;
  int second;
};

static_assert([](int value) {
  return value match { case int copy => copy; };
}(42) == 42);

static_assert([] {
  int value = 1;
  return value match { case int &ref => ++ref; };
}() == 2);

static_assert([] {
  return Pair{2, 3} match {
    case auto [first, second] => first + second;
  };
}() == 5);

static_assert([](int value) {
  return value match {
    case int copy if (copy > 0) => copy;
    case int copy => -copy;
  };
}(-7) == 7);

template<class T>
constexpr T dependent(T value) {
  return value match { case T copy => copy; };
}

template<class T>
constexpr T dependent_auto(T value) {
  return value match { case auto &&ref => ref; };
}

template<class T>
constexpr T dependent_guard(T value) {
  return value match {
    case T copy if (copy > T{}) => copy;
    case T copy => copy;
  };
}

template<class T>
constexpr int dependent_nested_declarations(T value) {
  return value match {
    case [auto &&first, auto &&second] => first;
  };
}

static_assert(dependent(11) == 11);
static_assert(dependent_auto(12) == 12);
static_assert(dependent_guard(13) == 13);
static_assert(dependent_nested_declarations(Pair{2, 3}) == 2);

struct ConstantSubject {
  int value;
};

constexpr bool constant_prvalue_subject_has_one_identity() {
  const ConstantSubject *saved = nullptr;
  return ConstantSubject{42} match {
    case auto &&value if ((saved = &value, false)) => false;
    case auto &&value => &value == saved;
  };
}

static_assert(constant_prvalue_subject_has_one_identity());

struct CopyCounter {
  int value;
  int *copies;

  constexpr CopyCounter(int value, int *copies)
      : value(value), copies(copies) {}
  constexpr CopyCounter(const CopyCounter &other)
      : value(other.value), copies(other.copies) {
    ++*copies;
  }
};

constexpr int failed_guard_copies_once() {
  int copies = 0;
  CopyCounter source{3, &copies};
  return source match {
    case CopyCounter copy if (false) => copy.value;
    case auto &&ref => copies + ref.value;
  };
}

constexpr int successful_guard_copies_once() {
  int copies = 0;
  CopyCounter source{3, &copies};
  return source match {
    case CopyCounter copy if (copy.value == 3) => copies;
    case _ => -1;
  };
}

static_assert(failed_guard_copies_once() == 4);
static_assert(successful_guard_copies_once() == 1);

constexpr int failed_match_test_guard_copies_once() {
  int copies = 0;
  CopyCounter source{3, &copies};
  bool matched = source match case CopyCounter copy if (false);
  return copies + matched;
}

constexpr int successful_match_test_guard_copies_once() {
  int copies = 0;
  CopyCounter source{3, &copies};
  bool matched = source match case CopyCounter copy if (copy.value == 3);
  return copies * 10 + matched;
}

static_assert(failed_match_test_guard_copies_once() == 1);
static_assert(successful_match_test_guard_copies_once() == 11);

struct LifetimeCounter {
  int *copies;
  int *destructions;

  constexpr LifetimeCounter(int *copies, int *destructions)
      : copies(copies), destructions(destructions) {}
  constexpr LifetimeCounter(const LifetimeCounter &other)
      : copies(other.copies), destructions(other.destructions) {
    ++*copies;
  }
  constexpr ~LifetimeCounter() { ++*destructions; }
};

constexpr int guarded_declaration_lifetime(bool guard) {
  int copies = 0;
  int destructions = 0;
  {
    LifetimeCounter source{&copies, &destructions};
    source match {
      case LifetimeCounter copy if (guard) => 0;
      case _ => 0;
    };
  }
  return copies * 10 + destructions;
}

static_assert(guarded_declaration_lifetime(false) == 12);
static_assert(guarded_declaration_lifetime(true) == 12);

constexpr int match_test_declaration_lifetime(bool guard) {
  int copies = 0;
  int destructions = 0;
  int observed_destructions;
  {
    LifetimeCounter source{&copies, &destructions};
    bool matched = source match case LifetimeCounter copy if (guard);
    observed_destructions = destructions + matched;
  }
  return copies * 100 + observed_destructions * 10 + destructions;
}

static_assert(match_test_declaration_lifetime(false) == 112);
static_assert(match_test_declaration_lifetime(true) == 122);

constexpr int structured_binding_guard_is_eager() {
  Pair pair{2, 3};
  return pair match {
    case auto [first, second] if (first < 0) => -1;
    case auto [first, second] => first + second;
  };
}

static_assert(structured_binding_guard_is_eager() == 5);

constexpr int structured_binding_guard_projects_once() {
  int projections = 0;
  GuardProjection source{7, &projections};
  return source match {
    case auto [value] if (value == 7) => projections;
    case _ => -1;
  };
}

constexpr int each_guarded_arm_projects_once() {
  int projections = 0;
  GuardProjection source{7, &projections};
  return source match {
    case auto [value] if (false) => -1;
    case auto [value] => projections;
  };
}

static_assert(structured_binding_guard_projects_once() == 1);
static_assert(each_guarded_arm_projects_once() == 2);

constexpr int structural_arms_share_projections() {
  int projections[2] = {};
  SharedProjection source{1, 2, projections};
  int result = source match {
    case [0, 0] => 0;
    case [auto &&x, 0] => x;
    case [0, auto &&y] => y;
    case [auto &&x, auto &&y] => x + y;
  };
  return projections[0] * 100 + projections[1] * 10 + result;
}

static_assert(structural_arms_share_projections() == 113);

constexpr int dependent_structural_arms_share_projections(auto &source) {
  return source match {
    case [0, 0] => 0;
    case [1, 0] => 1;
    case [0, 2] => 2;
    case [_, _] => 3;
  };
}

constexpr int instantiate_dependent_structural_projection() {
  int projections[2] = {};
  SharedProjection source{1, 2, projections};
  int result = dependent_structural_arms_share_projections(source);
  return projections[0] * 100 + projections[1] * 10 + result;
}

static_assert(instantiate_dependent_structural_projection() == 113);

constexpr int declaration_arms_share_projections() {
  int projections[2] = {};
  SharedProjection source{1, 2, projections};
  int result = source match {
    case auto &&[x, y] if (x == 0) => 0;
    case auto &&[x, y] => x + y;
  };
  return projections[0] * 100 + projections[1] * 10 + result;
}

static_assert(declaration_arms_share_projections() == 113);

constexpr int nested_arms_share_projections() {
  int element_projections[2] = {};
  int index_calls = 0;
  int alternative_projections = 0;
  SharedVariantProjection source{
      {1, 2, element_projections}, &index_calls, &alternative_projections};
  int result = source match {
    case { [0, 0] } => 0;
    case { [auto &&x, auto &&y] } => x + y;
  };
  return index_calls * 1000 + alternative_projections * 100 +
         element_projections[0] * 10 + element_projections[1] + result;
}

static_assert(nested_arms_share_projections() == 1114);

constexpr int guard_mutates_its_eager_declaration() {
  int subject = 1;
  int result = subject match {
    case int copy if ((copy = 3, true)) => copy;
    case _ => 0;
  };
  return subject * 10 + result;
}

static_assert(guard_mutates_its_eager_declaration() == 13);

struct ArmCopy {
  int value;
  int *copies;

  constexpr ArmCopy(int value, int *copies) : value(value), copies(copies) {}
  constexpr ArmCopy(const ArmCopy &other) : value(0), copies(other.copies) {
    ++*copies;
  }
};

constexpr int failed_guard_uses_a_fresh_copy_for_the_next_arm() {
  int copies = 0;
  ArmCopy subject{1, &copies};
  int result = subject match {
    case ArmCopy copy if (copy.value == 1) => 1;
    case ArmCopy copy => 2;
  };
  return copies * 10 + result;
}

static_assert(failed_guard_uses_a_fresh_copy_for_the_next_arm() == 22);

constexpr int failed_guard_does_not_reuse_binding_identity() {
  int subject = 1;
  const int *saved = nullptr;
  return subject match {
    case int copy if ((saved = &copy, false)) => 1;
    case int copy => &copy == saved ? 2 : 3;
  };
}

static_assert(failed_guard_does_not_reuse_binding_identity() == 3);

struct TrivialMoveState {
  int value;
};

constexpr int failed_guard_preserves_trivially_moved_subject() {
  TrivialMoveState subject{7};
  return static_cast<TrivialMoveState &&>(subject) match {
    case TrivialMoveState first if (false) => first.value;
    case TrivialMoveState second if (second.value == 7) => 1;
    case _ => 2;
  };
}

static_assert(failed_guard_preserves_trivially_moved_subject() == 1);

constexpr int subject_is_evaluated_once() {
  int evaluations = 0;
  return (++evaluations, 5) match {
    case int value if (value < 0) => 0;
    case int value => evaluations;
  };
}

static_assert(subject_is_evaluated_once() == 1);

} // namespace declaration_patterns

constexpr int match_subject_is_evaluated_once() {
  int evaluations = 0;
  return (++evaluations, 5) match {
    case auto&& value if (value < 0) => 0;
    case auto&& value => evaluations;
  };
}

static_assert(match_subject_is_evaluated_once() == 1);

struct ConstantMatchSubject {
  int value;
};

constexpr bool constant_prvalue_match_subject_has_one_identity() {
  const ConstantMatchSubject *saved = nullptr;
  return ConstantMatchSubject{42} match {
    case auto&& value if ((saved = &value, false)) => false;
    case auto&& value => &value == saved;
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
    case [auto&& x, 0] => x;
    case [0, auto&& y] => y;
    case [auto&& x, auto&& y] => x + y;
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
