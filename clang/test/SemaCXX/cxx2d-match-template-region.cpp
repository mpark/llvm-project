// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only -verify %s

namespace std {
template <class T> struct alternative_traits;
}

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template <unsigned I> struct ChoiceAlternative;
template <> struct ChoiceAlternative<0> { using type = int; };
template <> struct ChoiceAlternative<1> { using type = double; };

template <> struct std::alternative_traits<Choice> {
  static constexpr unsigned size = 2;
  template <unsigned I>
  using projection_type = typename ChoiceAlternative<I>::type;

  static constexpr unsigned index(const Choice &value) noexcept {
    return value.active;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(value).integer);
    else
      return (static_cast<Self &&>(value).real);
  }
};

int deferred_assertion(Choice value) {
  return value match {
    case { auto payload } => // expected-note 2{{while instantiating this pattern match}}
      [] { static_assert(false); return 0; }(); // expected-error 2{{static assertion failed}} expected-note 2{{while substituting into a lambda expression here}}
  };
}

int nondependent_lookup(Choice value) {
  return value match { // expected-error {{cannot initialize return object of type 'int' with an rvalue of type '<dependent type>'}}
    case { auto payload } => undeclared_name(); // expected-error {{use of undeclared identifier 'undeclared_name'}}
  };
}

constexpr int project(Choice value) {
  return value match {
    case { auto payload } => static_cast<int>(payload);
  };
}

static_assert(project({0, 42, 0}) == 42);
static_assert(project({1, 0, 24}) == 24);
