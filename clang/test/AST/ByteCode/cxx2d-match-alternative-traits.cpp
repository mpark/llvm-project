// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

inline constexpr int empty_state = 0;

namespace std {
template<class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
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
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  enum class names : __SIZE_TYPE__ { first = 0, second = 1 };

  static constexpr names index(const Choice& choice) noexcept {
    return names(choice.state);
  }

  template<names I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == names::first)
      return (static_cast<Self&&>(choice).first);
    else
      return (static_cast<Self&&>(choice).second);
  }
};

template<>
struct std::alternative_traits<TupleChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^OneElement, ^^TwoElements
  };
  static constexpr bool has_residual_states = false;

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

struct IndexOnlyChoice {
  unsigned state;
};

struct AnonymousProjection {
  int value;
};

struct VoidProjection {};

struct AliasedChoice {
  bool engaged;
  int value;
  int* index_calls;
};

struct GenericSelectorChoice {
  bool error;
  int value;
  long error_value;
};

struct DirectNamedChoice {
  bool second;
  int first_value;
  long second_value;
};

struct StateLabel {
  unsigned value;
};

inline constexpr StateLabel first_label{0};
inline constexpr StateLabel second_label{1};
inline constexpr StateLabel residual_label{2};

struct LabeledChoice {
  unsigned state;
};

struct DuplicateLabeledChoice {
  unsigned state;
};

struct ResidualLabeledChoice {
  unsigned state;

  friend constexpr bool operator==(ResidualLabeledChoice choice,
                                   StateLabel label) {
    return choice.state == label.value;
  }
};

template<>
struct std::alternative_traits<MaybeInt> {
  static constexpr alternative_info alternatives[] = {
    ^^int, {^^empty_state, true}
  };
  static constexpr bool has_residual_states = false;

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
struct std::alternative_traits<IndexOnlyChoice> {
  static constexpr alternative_info alternatives[] = {{}, {}};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(IndexOnlyChoice value) noexcept {
    return value.state;
  }
};

template<>
struct std::alternative_traits<AnonymousProjection> {
  static constexpr alternative_info alternatives[] = {{}};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(AnonymousProjection) noexcept { return 0; }

  template<auto I, class Self>
    requires (static_cast<__SIZE_TYPE__>(I) == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

template<>
struct std::alternative_traits<VoidProjection> {
  static constexpr alternative_info alternatives[] = {^^void};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(VoidProjection) noexcept { return 0; }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr void get(Self&&) {}
};

template<>
struct std::alternative_traits<AliasedChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, {^^empty_state, true}
  };
  static constexpr bool has_residual_states = false;

  enum class names : __SIZE_TYPE__ {
    value = 0,
    some = 0,
    error = 1,
    none = 1,
  };

  static constexpr names index(const AliasedChoice& choice) noexcept {
    ++*choice.index_calls;
    return choice.engaged ? names::value : names::none;
  }

  template<names Selector, class Self>
    requires (Selector == names::value)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

template<>
struct std::alternative_traits<GenericSelectorChoice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^long};
  static constexpr bool has_residual_states = false;

  enum discriminator : bool { value = false, error = true };

  static constexpr discriminator
  index(const GenericSelectorChoice& choice) noexcept {
    return choice.error ? discriminator::error : discriminator::value;
  }

  template<discriminator Selector, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (Selector == discriminator::error)
      return (static_cast<Self&&>(choice).error_value);
    else
      return (static_cast<Self&&>(choice).value);
  }
};

template<>
struct std::alternative_traits<DirectNamedChoice> {
  static constexpr alternative_info alternatives[] = {{}, {}};
  static constexpr bool has_residual_states = false;

  enum class names : bool { first = false, second = true };

  static constexpr names index(const DirectNamedChoice& choice) noexcept {
    return choice.second ? names::second : names::first;
  }

  template<names Selector, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (Selector == names::first)
      return (static_cast<Self&&>(choice).first_value);
    else
      return (static_cast<Self&&>(choice).second_value);
  }
};

template<>
struct std::alternative_traits<LabeledChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^first_label, ^^second_label
  };
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(LabeledChoice choice) noexcept {
    return choice.state;
  }
};

template<>
struct std::alternative_traits<DuplicateLabeledChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^first_label, ^^first_label
  };
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(DuplicateLabeledChoice choice) noexcept {
    return choice.state;
  }
};

template<>
struct std::alternative_traits<ResidualLabeledChoice> {
  static constexpr alternative_info alternatives[] = {^^first_label};
  static constexpr bool has_residual_states = true;

  static constexpr unsigned index(ResidualLabeledChoice choice) noexcept {
    return choice.state;
  }
};

constexpr int match_choice(Choice choice) {
  return match (choice) {
    case { .first: int value } => value;
    case { .second: double value } => static_cast<int>(value) + 10;
  };
}

constexpr int match_maybe(MaybeInt value) {
  return match (value) {
    case { int number } => number;
    case {} => -1;
  };
}

constexpr int classify(const int&) { return 1; }
constexpr int classify(const double&) { return 2; }

constexpr int match_generic(Choice choice) {
  return match (choice) {
    case { auto&& value } => classify(value);
  };
}

constexpr int match_type_selector(Choice choice) {
  return match (choice) {
    case { int: 0 } => 20;
    case { int: auto value } => value;
    case { double: auto value } => static_cast<int>(value) + 10;
  };
}

template<class T>
concept Integral = __is_integral(T);

template<class T, class U>
concept SameAs = __is_same(T, U);

constexpr int match_type_constraint_selector(Choice choice) {
  return match (choice) {
    case { Integral: auto value } => value;
    case { SameAs<double>: auto value } => static_cast<int>(value) + 10;
  };
}

template<class T>
constexpr int match_dependent_type_constraint_selector(Choice choice) {
  return match (choice) {
    case { SameAs<T>: auto value } => static_cast<int>(value);
    case { _ } => -1;
  };
}

struct ChoiceWithTail {
  Choice choice;
  int tail;
};

constexpr int match_nested_type_constraint_selector(ChoiceWithTail value) {
  return match (value) {
    case [{ Integral: auto head }, auto tail] => head + tail;
    case _ => -1;
  };
}

constexpr int match_expression_selector(Choice choice) {
  return match (choice) {
    case { .[0]: int value } => value;
    case { .[1]: double value } => static_cast<int>(value) + 10;
  };
}

static_assert(match_type_constraint_selector({0, 4, 0.0}) == 4);
static_assert(match_type_constraint_selector({1, 0, 2.5}) == 12);
static_assert(match_dependent_type_constraint_selector<int>({0, 5, 0.0}) == 5);
static_assert(match_dependent_type_constraint_selector<int>({1, 0, 3.0}) == -1);
static_assert(match_dependent_type_constraint_selector<double>({1, 0, 3.0}) == 3);
static_assert(match_nested_type_constraint_selector({{0, 4, 0.0}, 5}) == 9);
static_assert(match_nested_type_constraint_selector({{1, 0, 2.5}, 5}) == -1);

template<class T>
constexpr int match_dependent_type_selector(Choice choice) {
  return match (choice) {
    case { T: auto value } => static_cast<int>(value);
    case _ => -1;
  };
}

template<__SIZE_TYPE__ I>
constexpr int match_dependent_expression_selector(Choice choice) {
  return match (choice) {
    case { .[I]: auto value } => static_cast<int>(value);
    case _ => -1;
  };
}

constexpr int match_generic_binding_pack(TupleChoice choice) {
  return match (choice) {
    case { auto [...elements] } => (... + elements);
  };
}

constexpr int match_generic_declaration_pack(TupleChoice choice) {
  return match (choice) {
    case { [auto&& ...elements] } =>
        int(sizeof...(elements)) + (... + elements);
  };
}

constexpr int match_generic_wildcard_pack(TupleChoice choice) {
  return match (choice) {
    case { [auto&& first, ...] } => first;
  };
}

constexpr int aliases_share_one_discriminator() {
  int index_calls = 0;
  AliasedChoice choice{true, 2, &index_calls};
  int result = match (choice) {
    case { .value: 0 } => 0;
    case { .some: 1 } => 1;
    case { .some: int value } => value;
    case { .none } => -1;
  };
  return index_calls * 100 + result;
}

constexpr int match_generic_selector_default(GenericSelectorChoice choice) {
  return match (choice) {
    case { int value } => value;
    case { long error } => static_cast<int>(error) + 10;
  };
}

constexpr int match_generic_selector_named(GenericSelectorChoice choice) {
  return match (choice) {
    case { .value: int value } => value;
    case { .error: long error } => static_cast<int>(error) + 20;
  };
}

constexpr int match_direct_named(DirectNamedChoice choice) {
  return match (choice) {
    case { .first: int value } => value;
    case { .second: long value } => static_cast<int>(value) + 30;
  };
}

constexpr int match_labeled(LabeledChoice choice) {
  return match (choice) {
    case first_label => 10;
    case second_label => 20;
  };
}

constexpr int match_duplicate_label(DuplicateLabeledChoice choice) {
  return match (choice) {
    case first_label => 30;
  };
}

constexpr int match_residual_label(ResidualLabeledChoice choice) {
  return match (choice) {
    case first_label => 40;
    case residual_label => 50;
    case _ => 60;
  };
}

template<class T>
constexpr int match_dependent_generic(T& choice) {
  return match (choice) {
    case { auto&& value } => classify(value);
  };
}

template<class T>
constexpr bool failed_dependent_guard_is_evaluated_once(T& choice) {
  int guards = 0;
  int result = match (choice) {
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
static_assert(match (IndexOnlyChoice{0}) {
  case { .[0] } => true;
  case { .[1] } => false;
});
static_assert(match (IndexOnlyChoice{1}) {
  case { .[0] } => false;
  case { .[1] } => true;
});

template<unsigned I>
constexpr bool index_only_state(IndexOnlyChoice choice) {
  return match(choice, case { .[I] });
}

static_assert(index_only_state<0>({0}));
static_assert(!index_only_state<0>({1}));
static_assert(index_only_state<1>({1}));
constexpr int match_anonymous_by_index(AnonymousProjection choice) {
  return match (choice) {
    case { .[0]: int value } => value;
  };
}

constexpr int match_anonymous_generically(AnonymousProjection choice) {
  return match (choice) {
    case { int value } => value;
  };
}

static_assert(match_anonymous_by_index({42}) == 42);
static_assert(match_anonymous_generically({42}) == 42);
static_assert(match (VoidProjection{}) {
  case { void: _ } => true;
});
static_assert(match_generic({0, 3, 4}) == 1);
static_assert(match_generic({1, 3, 4}) == 2);
static_assert(match_type_selector({0, 0, 4}) == 20);
static_assert(match_type_selector({0, 3, 4}) == 3);
static_assert(match_type_selector({1, 3, 4}) == 14);
static_assert(match_expression_selector({0, 3, 4}) == 3);
static_assert(match_expression_selector({1, 3, 4}) == 14);
static_assert(match_dependent_type_selector<int>({0, 3, 4}) == 3);
static_assert(match_dependent_type_selector<int>({1, 3, 4}) == -1);
static_assert(match_dependent_expression_selector<0>({0, 3, 4}) == 3);
static_assert(match_dependent_expression_selector<0>({1, 3, 4}) == -1);
static_assert(match_generic_binding_pack({0, {3}, {4, 5}}) == 3);
static_assert(match_generic_binding_pack({1, {3}, {4, 5}}) == 9);
static_assert(match_generic_declaration_pack({0, {3}, {4, 5}}) == 4);
static_assert(match_generic_declaration_pack({1, {3}, {4, 5}}) == 11);
static_assert(match_generic_wildcard_pack({0, {3}, {4, 5}}) == 3);
static_assert(match_generic_wildcard_pack({1, {3}, {4, 5}}) == 4);
static_assert(aliases_share_one_discriminator() == 102);
static_assert(match_generic_selector_default({false, 4, 5}) == 4);
static_assert(match_generic_selector_default({true, 4, 5}) == 15);
static_assert(match_generic_selector_named({false, 6, 7}) == 6);
static_assert(match_generic_selector_named({true, 6, 7}) == 27);
static_assert(match_direct_named({false, 10, 11}) == 10);
static_assert(match_direct_named({true, 10, 11}) == 41);
static_assert(match_labeled({0}) == 10);
static_assert(match_labeled({1}) == 20);
static_assert(match_duplicate_label({0}) == 30);
static_assert(match_duplicate_label({1}) == 30);
static_assert(match_residual_label({0}) == 40);
static_assert(match_residual_label({1}) == 60);
static_assert(match_residual_label({2}) == 50);
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
