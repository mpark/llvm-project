// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value %s -verify

void test_throw_does_not_contribute_to_type_deduction() {
  static_assert(__is_same(decltype(0 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  }), int));
}

void test_throw_action() {
  static_assert(0 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 0);
  static_assert(1 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 1);
}

void test_decomposition_pattern_arity() {
  struct S { int a; int b; };
  S s{1, 2};
  s match {
    case [1, 2, 3] => 0; // expected-error {{type 'S' binds to 2 elements, but 3 names were provided}}
    case _ => 0;
  };
}

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
  struct variant_size<const T> {
    static constexpr int value = std::variant_size<T>::value;
  };

  template <>
  struct variant_size<Variant> {
    static constexpr int value = 3;
  };

  template <int I, typename T>
  struct variant_alternative; // expected-note {{template is declared here}}

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
    case int: _ => 0;
    case short: _ => 1; // expected-error {{no viable alternative; target type 'short' does not match any 'std::variant_alternative<I, Variant>::type' for I in [0, 'std::variant_size<Variant>::value')}}
    case _ => -1;
  };
}

struct S1 {};

template <>
struct std::variant_size<S1> { void value(); };

int test_bad_variant_like_protocol_variant_size_value() {
  return S1{} match {
    case int: _ => 0; // expected-error {{invalid variant-like protocol; 'std::variant_size<S1>::value' is not a valid integral constant expression}}
    case _ => -1;
  };
}

struct S2 {};

template <>
struct std::variant_size<S2> { static constexpr int value = 1; };

int test_bad_variant_like_protocol_missing_index() {
  return S2{} match {
    case int: _ => 0; // expected-error {{use of undeclared identifier 'index'}}
    case _ => -1;
  };
}

struct S3 {
  int index() const { return 0; }
};

template <>
struct std::variant_size<S3> { static constexpr int value = 1; };

int test_bad_variant_like_protocol_missing_variant_alternative() {
  return S3{} match {
    case int: _ => 0; // expected-error {{implicit instantiation of undefined template 'std::variant_alternative<0, S3>'}}
    case _ => -1;
  };
}

struct S4 {
  int index() const { return 0; }
};

template <>
struct std::variant_size<S4> { static constexpr int value = 1; };

template <>
struct std::variant_alternative<0, S4> {};

int test_bad_variant_like_protocol_variant_alternative_missing_type() {
  return S4{} match {
    case int: _ => 0; // expected-error {{invalid variant-like protocol; 'std::variant_alternative<0UL, S4>::type' does not name a type}}
    case _ => -1;
  };
}

namespace declaration_patterns {

struct Pair {
  int first;
  int second;
};

struct Guarded {
  int value;
};

struct Movable {
  int value;
  Movable(const Movable &);
  Movable(Movable &&);
};

struct MovePair {
  Movable first;
  Movable second;
};

bool sees_mutable(Guarded &);
bool sees_mutable(const Guarded &) = delete;

int basic(int value) {
  return value match {
    case int copy => copy;
  };
}

int reference(int &value) {
  return value match {
    case int &ref => ++ref;
  };
}

int forwarding(int &&value) {
  return static_cast<int &&>(value) match {
    case auto &&ref => ref;
  };
}

int decomposition(Pair pair) {
  return pair match {
    case auto [first, second] => first + second;
  };
}

int attributed(int value) {
  return value match {
    case [[maybe_unused]] int copy => copy;
  };
}

int attributed_decomposition(Pair pair) {
  return pair match {
    case [[maybe_unused]] auto [first, second] => first + second;
  };
}

int guard(int value) {
  return value match {
    case int copy if (copy > 0) => copy;
    case int copy => -copy;
  };
}

int mutable_guard(Guarded value) {
  return value match {
    case Guarded copy if (__is_same(decltype(copy), Guarded) &&
                     __is_same(decltype((copy)), Guarded &) &&
                     sees_mutable(copy)) => (copy.value = 1);
    case Guarded copy => copy.value;
  };
}

int guard_can_mutate_declaration(Guarded value) {
  return value match {
    case Guarded copy if ((copy.value = 1)) => copy.value;
    case _ => 0;
  };
}

int mutable_structured_binding_guard(Pair pair) {
  return pair match {
    case auto [first, second]
        if (__is_same(decltype(first), int) &&
            __is_same(decltype((first)), int &) && first > 0) =>
        (first = 0, second);
    case auto [first, second] => first + second;
  };
}

struct PointerMember {
  int *pointer;
};

int guard_can_mutate_pointee(PointerMember member) {
  return member match {
    case auto [pointer] if ((*pointer = 3, true)) => *pointer;
    case _ => 0;
  };
}

int guarded_move(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable moved if (true) => moved.value; // expected-error {{guarded declaration pattern of type 'Movable' would move from the match subject}}
    case _ => 0;
  };
}

int guarded_structured_binding_move(MovePair &&value) {
  return static_cast<MovePair &&>(value) match {
    case auto [first, second] if (true) => first.value + second.value; // expected-error {{guarded declaration pattern of type 'MovePair' would move from the match subject}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_move(Movable (&&value)[1]) {
  return static_cast<Movable (&&)[1]>(value) match {
    case auto [element] if (true) => element.value; // expected-error {{guarded declaration pattern of type 'Movable[1]' would move from the match subject}}
    case _ => 0;
  };
}

int guarded_nested_array_structured_binding_move(Movable (&&value)[1][1]) {
  return static_cast<Movable (&&)[1][1]>(value) match {
    case auto [row] if (true) => row[0].value; // expected-error {{guarded declaration pattern of type 'Movable[1][1]' would move from the match subject}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_copy(Movable (&value)[1]) {
  return value match {
    case auto [element] if (true) => element.value;
    case _ => 0;
  };
}

int unguarded_array_structured_binding_move(Movable (&&value)[1]) {
  return static_cast<Movable (&&)[1]>(value) match {
    case auto [element] => element.value;
  };
}

int unguarded_move(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable moved => moved.value;
  };
}

int guarded_rvalue_reference(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable &&ref if (ref.value > 0) => ref.value;
    case _ => 0;
  };
}

template<class T>
int forwarding_guard(T &&value) {
  return static_cast<T &&>(value) match {
    case T copy if (true) => 1; // expected-error {{guarded declaration pattern of type 'declaration_patterns::Movable' would move from the match subject}}
    case _ => 0;
  };
}

int instantiate_forwarding_guard(Movable &lvalue, Movable &&rvalue) {
  return forwarding_guard(lvalue) +
         forwarding_guard(static_cast<Movable &&>(rvalue)); // expected-note {{in instantiation of function template specialization}}
}

template<class T>
T dependent(T value) {
  return value match {
    case T copy => copy;
  };
}

template<class T>
constexpr T dependent_guard(T value) {
  return value match {
    case T copy if (__is_same(decltype(copy), T) &&
               __is_same(decltype((copy)), T &) && copy > T{}) => copy;
    case T copy => copy;
  };
}

template<class T>
constexpr int dependent_decomposition_guard(T value) {
  return value match {
    case auto [first, second]
        if (__is_same(decltype((first)), int &) && first > 0) =>
        first + second;
    case _ => 0;
  };
}

static_assert(dependent_guard(3) == 3);
static_assert(dependent_decomposition_guard(Pair{2, 3}) == 5);

template<class T>
T dependent_auto(T value) {
  return value match {
    case auto &&ref => ref;
  };
}

static_assert(__is_same(decltype(dependent(1)), int));
static_assert(__is_same(decltype(dependent_auto(1)), int));

int bad_conversion(int value) {
  return value match {
    case char converted => converted; // expected-error {{declaration pattern of type 'char' is not an exact match for subject of type 'int'}}
    case _ => 0;
  };
}

int bad_promotion(char value) {
  return value match {
    case int promoted => promoted; // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'char'}}
    case _ => 0;
  };
}

int bad_storage(int value) {
  return value match {
    case static int copy => copy; // expected-error {{loop variable 'copy' may not be declared 'static'}}
    case _ => 0;
  };
}

} // namespace declaration_patterns
