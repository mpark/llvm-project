// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

namespace std {
template<class T>
struct alternative_traits;
}

struct Choice {
  unsigned state;
  int first;
  double second;
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
    static constexpr __SIZE_TYPE__ first = 0;
    static constexpr __SIZE_TYPE__ second = 1;
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
  while (case { auto&& value } = choice if (value-- > 0))
    ++count;
  return count;
}

constexpr int match_condition_for(Choice choice) {
  int count = 0;
  for (; case { auto&& value } = choice if (value < 3); ++value) {
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
