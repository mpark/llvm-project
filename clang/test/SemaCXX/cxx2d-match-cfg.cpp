// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wreturn-type -Wuninitialized \
// RUN:   -verify %s

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

enum Gapped { Zero = 0, Two = 2 };

int required_but_not_fully_covered(Gapped value) {
  value match {
    case Zero => return 0;
    case Two => return 2;
  };
} // expected-warning {{non-void function does not return a value in all control paths}}

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

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
  using type = __type_pack_element<I, int, double>;

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
  if (case { auto&& value } = choice)
    return classify(value);
  else
    return 0;
}

int projected_guard(Choice& choice) {
  if (case { auto&& value } = choice) {
    if (classify(value) != 0)
      return classify(value);
  }
  return 0;
}

int projected_condition_chain(Choice& first, Choice& second) {
  if (first.active < 2 && case { auto&& value } = first &&
      classify(value) > 0 && case { auto&& other } = second &&
      classify(other) > classify(value))
    return classify(value) + classify(other);
  return -1;
}

template<class V>
int projected_constexpr_condition_chain() {
  constexpr V value{0, 42, 0.0};
  if constexpr (case { auto projected } = value && projected == 42)
    return static_cast<int>(projected);
  else
    return -1;
}

template int projected_constexpr_condition_chain<Choice>();
