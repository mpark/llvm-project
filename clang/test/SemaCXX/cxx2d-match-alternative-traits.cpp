// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

namespace std {
template<class T>
struct alternative_traits; // expected-note {{template is declared here}}
}

struct Choice {
  unsigned state;
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

  struct names {
    enum { integer, real };
  };

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).integer);
    else
      return (static_cast<Self&&>(choice).real);
  }
};

struct ProjectedNonCopyable {
  ProjectedNonCopyable();
  ProjectedNonCopyable(const ProjectedNonCopyable&) = delete; // expected-note 2{{has been explicitly marked deleted here}}
};

struct CopyChoice {
  bool engaged;
  ProjectedNonCopyable value;
};

template<>
struct std::alternative_traits<CopyChoice> {
  static constexpr __SIZE_TYPE__ size = 2;

  static constexpr __SIZE_TYPE__ index(const CopyChoice& choice) noexcept {
    return choice.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
};

template<>
struct std::alternative_traits<MaybeInt> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  using projection_type = int;

  static constexpr __SIZE_TYPE__ index(const MaybeInt& value) noexcept {
    return value.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& value) {
    return (static_cast<Self&&>(value).value);
  }
};

struct ThrowingIndex {
  int value;
};

template<>
struct std::alternative_traits<ThrowingIndex> {
  static constexpr __SIZE_TYPE__ size = 1;

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  using projection_type = int;

  struct names {
    static constexpr __SIZE_TYPE__ value = 0;
  };
  static __SIZE_TYPE__ index(const ThrowingIndex&);

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  static int& get(ThrowingIndex& choice) {
    return choice.value;
  }
};

bool throwing_index(ThrowingIndex choice) {
  return choice match case { .value: int value }; // expected-error {{invalid alternative protocol; 'std::alternative_traits<'ThrowingIndex'>::index' must be noexcept}}
}

int named(Choice choice) {
  return choice match {
    case { .integer: int value } => value;
    case { .real: double value } => static_cast<int>(value);
  };
}

int generic_binding(Choice choice) {
  return choice match {
    case { int value } => value;
    case { double value } => static_cast<int>(value);
  };
}

int projected_deleted_copy_does_not_fall_back(const CopyChoice& choice) {
  return choice match {
    case { auto copy } => 1; // expected-error {{call to deleted constructor of 'ProjectedNonCopyable'}}
    case _ => 0;
  };
}

int projected_deleted_hypothetical_copy_is_invalid(const CopyChoice& choice) {
  return choice match {
    case { ProjectedNonCopyable } => 1; // expected-error {{call to deleted constructor of 'ProjectedNonCopyable'}}
    case _ => 0;
  };
}

int empty(MaybeInt value) {
  return value match {
    case { int number } => number;
    case {} => -1;
  };
}

int bad_name(Choice choice) {
  return choice match {
    case { .missing: int value } => value; // expected-error {{alternative name 'missing' is not defined}}
    case _ => 0;
  };
}

int bad_empty(Choice choice) {
  return choice match {
    case {} => 0; // expected-error {{type 'Choice' has no non-projectable alternative state}}
    case _ => 1;
  };
}

int classify(int&);
int classify(double&);

int generic(Choice choice) {
  return choice match {
    case { auto&& value } => classify(value);
  };
}

int no_viable_alternative(Choice choice) {
  return choice match {
    case { char value } => value; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 0;
  };
}

int no_structural_alternative(Choice choice) {
  return choice match {
    case { auto&& [first, second] } => first; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 0;
  };
}

struct ChoicePair {
  Choice first;
  int second;
};

int direct_generic_alternative(ChoicePair pair) {
  if (pair match case [{ auto&& value }, _])
    return classify(value);
  return 0;
}

int direct_generic_guard(ChoicePair pair) {
  if (pair match case [{ auto&& value }, _] if (classify(value) != 0))
    return classify(value);
  else
    return 0;
}

bool nested_match_subject(ChoicePair pair) {
  return (pair match case [{ auto&& value }, _]) match case true;
}

template<class T>
concept CanDirectlyMatchChar = requires(T value) {
  value match case [{ char c }, _];
};

static_assert(!CanDirectlyMatchChar<ChoicePair>);

bool invalid_direct_alternative(ChoicePair pair) {
  return pair match case [{ char c }, _]; // expected-error {{braced alternative pattern does not match any projectable state of 'ChoicePair'}}
}

struct NoTraits {};

int missing_traits(NoTraits value) {
  return value match {
    case { NoTraits copy } => 0; // expected-error {{implicit instantiation of undefined template 'std::alternative_traits<NoTraits>'}}
    case _ => 1;
  };
}
