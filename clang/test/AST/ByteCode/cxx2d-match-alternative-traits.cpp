// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

namespace std {
template<class T>
struct alternative_traits;

template<class Provider>
struct alternative_name {
  using provider = Provider;
  __SIZE_TYPE__ index;
  consteval alternative_name(__SIZE_TYPE__ index) : index(index) {}
};
}

struct Choice {
  unsigned state;
  int first;
  double second;
};

struct OneElement {
  int first;
};

struct TwoElements {
  int first;
  int second;
};

struct TupleChoice {
  unsigned state;
  OneElement one;
  TwoElements two;
};

template<>
struct std::alternative_traits<Choice> {
  using AT = alternative_traits;
  static constexpr __SIZE_TYPE__ size = 2;

  struct names {
    static constexpr alternative_name<AT> first = 0, second = 1;
  };

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).first);
    else
      return (static_cast<Self&&>(choice).second);
  }
};

template<>
struct std::alternative_traits<TupleChoice> {
  static constexpr __SIZE_TYPE__ size = 2;

  static constexpr __SIZE_TYPE__ index(const TupleChoice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).one);
    else
      return (static_cast<Self&&>(choice).two);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
};

struct MultiViewChoice {
  bool engaged;
  int value;
  int* primary_index_calls;
  int* nullable_index_calls;
};

struct NullableChoiceView {
  static constexpr __SIZE_TYPE__ size = 2;
  static constexpr bool is_exhaustive = true;

  static constexpr __SIZE_TYPE__ index(const MultiViewChoice& choice) noexcept {
    ++*choice.nullable_index_calls;
    return choice.engaged ? 1 : 0;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 1)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

template<>
struct std::alternative_traits<MaybeInt> {
  static constexpr __SIZE_TYPE__ size = 2;

  static constexpr __SIZE_TYPE__ index(const MaybeInt& value) noexcept {
    return value.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& value) {
    return (static_cast<Self&&>(value).value);
  }
};

template<>
struct std::alternative_traits<MultiViewChoice> {
  using AT = alternative_traits;
  static constexpr __SIZE_TYPE__ size = 2;
  static constexpr bool is_exhaustive = true;

  static constexpr __SIZE_TYPE__ index(const MultiViewChoice& choice) noexcept {
    ++*choice.primary_index_calls;
    return choice.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }

  struct names {
    static constexpr alternative_name<AT> value = 0, error = 1;
    static constexpr alternative_name<NullableChoiceView> none = 0, some = 1;
  };
};

constexpr int match_choice(Choice choice) {
  return choice match {
    case { .first: int value } => value;
    case { .second: double value } => static_cast<int>(value) + 10;
  };
}

constexpr int match_maybe(MaybeInt value) {
  return value match {
    case { int number } => number;
    case {} => -1;
  };
}

constexpr int classify(const int&) { return 1; }
constexpr int classify(const double&) { return 2; }

constexpr int match_generic(Choice choice) {
  return choice match {
    case { auto&& value } => classify(value);
  };
}

constexpr int match_generic_binding_pack(TupleChoice choice) {
  return choice match {
    case { auto [...elements] } => (... + elements);
  };
}

constexpr int match_generic_declaration_pack(TupleChoice choice) {
  return choice match {
    case { [auto&& ...elements] } =>
        int(sizeof...(elements)) + (... + elements);
  };
}

constexpr int match_generic_wildcard_pack(TupleChoice choice) {
  return choice match {
    case { [auto&& first, ..._] } => first;
  };
}

constexpr int multiple_views_cache_each_discriminator() {
  int primary_index_calls = 0;
  int nullable_index_calls = 0;
  MultiViewChoice choice{
      true, 2, &primary_index_calls, &nullable_index_calls};
  int result = choice match {
    case { .value: 0 } => 0;
    case { .some: 1 } => 1;
    case { .some: int value } => value;
    case { .none } => -1;
  };
  return primary_index_calls * 100 + nullable_index_calls * 10 + result;
}

template<class T>
constexpr int match_dependent_generic(T& choice) {
  return choice match {
    case { auto&& value } => classify(value);
  };
}

template<class T>
constexpr bool failed_dependent_guard_is_evaluated_once(T& choice) {
  int guards = 0;
  int result = choice match {
    case { auto&& value } if (++guards == 2) => classify(value);
    case _ => 0;
  };
  return result == 0 && guards == 1;
}

constexpr int match_condition_if(Choice choice) {
  if (case { auto&& value } = choice)
    return classify(value);
  return 0;
}

constexpr int match_condition_while(Choice choice) {
  int count = 0;
  while (case { auto&& value } = choice) {
    if (value-- <= 0)
      break;
    ++count;
  }
  return count;
}

constexpr int match_condition_for(Choice choice) {
  int count = 0;
  for (; case { auto&& value } = choice; ++value) {
    if (value >= 3)
      break;
    ++count;
    if (value == 1)
      continue;
  }
  return count;
}

template<class C>
constexpr int dependent_match_condition_if(C choice) {
  if (case { auto&& value } = choice)
    return classify(value);
  return 0;
}

template<class T>
constexpr int dependent_pattern_condition_if(Choice choice) {
  if (case { T value } = choice)
    return sizeof(value);
  return 0;
}

static_assert(match_choice({0, 3, 4}) == 3);
static_assert(match_choice({1, 3, 4}) == 14);
static_assert(match_maybe({true, 5}) == 5);
static_assert(match_maybe({false, 5}) == -1);
static_assert(match_generic({0, 3, 4}) == 1);
static_assert(match_generic({1, 3, 4}) == 2);
static_assert(match_generic_binding_pack({0, {3}, {4, 5}}) == 3);
static_assert(match_generic_binding_pack({1, {3}, {4, 5}}) == 9);
static_assert(match_generic_declaration_pack({0, {3}, {4, 5}}) == 4);
static_assert(match_generic_declaration_pack({1, {3}, {4, 5}}) == 11);
static_assert(match_generic_wildcard_pack({0, {3}, {4, 5}}) == 3);
static_assert(match_generic_wildcard_pack({1, {3}, {4, 5}}) == 4);
static_assert(multiple_views_cache_each_discriminator() == 112);
constexpr Choice dependent_first{0, 3, 4};
constexpr Choice dependent_second{1, 3, 4};
static_assert(match_dependent_generic(dependent_first) == 1);
static_assert(match_dependent_generic(dependent_second) == 2);
static_assert(failed_dependent_guard_is_evaluated_once(dependent_first));
static_assert(failed_dependent_guard_is_evaluated_once(dependent_second));
static_assert(match_condition_if({0, 3, 4}) == 1);
static_assert(match_condition_if({1, 3, 4}) == 2);
static_assert(match_condition_while({0, 3, 4}) == 3);
static_assert(match_condition_while({1, 3, 2}) == 2);
static_assert(match_condition_for({0, 0, 4}) == 3);
static_assert(match_condition_for({1, 0, 1}) == 2);
static_assert(dependent_match_condition_if(Choice{0, 3, 4}) == 1);
static_assert(dependent_match_condition_if(Choice{1, 3, 4}) == 2);
static_assert(dependent_pattern_condition_if<int>({0, 3, 4}) == sizeof(int));
static_assert(dependent_pattern_condition_if<int>({1, 3, 4}) == 0);
static_assert(dependent_pattern_condition_if<double>({1, 3, 4}) ==
              sizeof(double));
