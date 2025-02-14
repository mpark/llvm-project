// RUN: %clang_cc1 -std=c++2c -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value %s -verify

void test_throw_does_not_contribute_to_type_deduction() {
  static_assert(__is_same(decltype(0 match {
    0 => 0;
    1 => 1;
    _ => throw;
  }), int));
}

void test_throw_action() {
  static_assert(0 match {
    0 => 0;
    1 => 1;
    _ => throw;
  } == 0);
  static_assert(1 match {
    0 => 0;
    1 => 1;
    _ => throw;
  } == 1);
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
    int: _ => 0;
    short: _ => 1; // expected-error {{no viable alternative; target type 'short' does not match any 'std::variant_alternative<I, Variant>::type' for I in [0, 'std::variant_size<Variant>::value')}}
    _ => -1;
  };
}

struct S1 {};

template <>
struct std::variant_size<S1> { void value(); };

int test_bad_variant_like_protocol_variant_size_value() {
  return S1{} match {
    int: _ => 0; // expected-error {{invalid variant-like protocol; 'std::variant_size<S1>::value' is not a valid integral constant expression}}
    _ => -1;
  };
}

struct S2 {};

template <>
struct std::variant_size<S2> { static constexpr int value = 1; };

int test_bad_variant_like_protocol_missing_index() {
  return S2{} match {
    int: _ => 0; // expected-error {{use of undeclared identifier 'index'}}
    _ => -1;
  };
}

struct S3 {
  int index() const { return 0; }
};

template <>
struct std::variant_size<S3> { static constexpr int value = 1; };

int test_bad_variant_like_protocol_missing_variant_alternative() {
  return S3{} match {
    int: _ => 0; // expected-error {{implicit instantiation of undefined template 'std::variant_alternative<0, S3>'}}
    _ => -1;
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
    int: _ => 0; // expected-error {{invalid variant-like protocol; 'std::variant_alternative<0UL, S4>::type' does not name a type}}
    _ => -1;
  };
}
