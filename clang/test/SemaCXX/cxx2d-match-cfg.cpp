// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wreturn-type -Wuninitialized \
// RUN:   -verify %s

// expected-no-diagnostics

int exhaustive(bool value) {
  value match {
    case true => return 1;
    case false => return 0;
  };
}

int guarded(int value) {
  return value match {
    case auto&& copy if (copy > 0) => copy;
    case _ => 0;
  };
}

int declaration(int value) {
  value match {
    case int copy => return copy;
  };
}

int guarded_init_statement(int value) {
  return value match {
    case int copy if (int adjusted = copy + 1; adjusted > 0) => adjusted;
    case _ => 0;
  };
}

namespace std {
template<class T>
struct alternative_traits;
}

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template<__SIZE_TYPE__ I>
struct ChoiceAlternative;

template<>
struct ChoiceAlternative<0> {
  using type = int;
};

template<>
struct ChoiceAlternative<1> {
  using type = double;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
  using projection_type = typename ChoiceAlternative<I>::type;

  static constexpr __SIZE_TYPE__ index(const Choice& value) noexcept {
    return value.active;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& value) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(value).integer);
    else
      return (static_cast<Self&&>(value).real);
  }
};

int classify(int&);
int classify(double&);

int projected_if(Choice& choice) {
  if (choice match case { auto&& value })
    return classify(value);
  else
    return 0;
}

int projected_guard(Choice& choice) {
  if (choice match case { auto&& value } if (classify(value) != 0))
    return classify(value);
  return 0;
}
