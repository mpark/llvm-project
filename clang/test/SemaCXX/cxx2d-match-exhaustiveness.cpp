// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s -verify

enum class EmptyState { first, second };
inline constexpr int nullable_empty_state = 0;

int missing_bool(bool b) {
  return b match { // expected-error {{match expression is not exhaustive; example of a missing case: false}}
    case true => 1;
  };
}

int exhaustive_bool(bool b) {
  return b match {
    case false => 0;
    case true => 1;
  };
}

int guarded_case_is_not_coverage(bool b) {
  return b match { // expected-error {{match expression is not exhaustive; example of a missing case: false}}
    case _ if (int guard = 0; guard == 0) => 0;
    case true => 1;
  };
}

int redundant_bool(bool b) {
  return b match {
    case _ => 0;
    case true => 1; // expected-error {{match case is redundant}}
  };
}

int guarded_case_after_wildcard_is_redundant(bool b) {
  return b match {
    case _ => 0;
    case true if (b) => 1; // expected-error {{match case is redundant}}
  };
}

int guarded_case_after_same_value_is_redundant(bool b) {
  return b match {
    case true => 0;
    case true if (b) => 1; // expected-error {{match case is redundant}}
    case false => 2;
  };
}

int guarded_cases_do_not_make_later_cases_redundant(bool b) {
  return b match {
    case true if (b) => 0;
    case true if (!b) => 1;
    case true => 2;
    case false => 3;
  };
}

int missing_integer(int value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: 1}}
    case 0 => 0;
  };
}

template<class T>
int dependent_missing_integer(T value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: 1}}
    case 0 => 0;
  };
}

int instantiate_dependent_missing_integer() {
  return dependent_missing_integer(1); // expected-note {{in instantiation of function template specialization 'dependent_missing_integer<int>' requested here}}
}

int exhaustive_integer(int value) {
  return value match {
    case 0 => 0;
    case _ => 1;
  };
}

int exhaustive_integer_with_default(int value) {
  return value match {
    case 0 => 0;
    default => 1;
  };
}

int case_after_default_is_redundant(bool value) {
  return value match {
    default => 0;
    case true => 1; // expected-error {{match case is redundant}}
  };
}

int default_after_complete_domain_is_redundant(bool value) {
  return value match {
    case false => 0;
    case true => 1;
    default => 2; // expected-error {{match case is redundant}}
  };
}

int redundant_integer_catch_all(int value) {
  return value match {
    case _ => 0;
    case _ => 1; // expected-error {{match case is redundant}}
  };
}

int redundant_integer_value(int value) {
  return value match {
    case 0 => 0;
    case 0 => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

int impossible_promoted_integer_value(unsigned char value) {
  return value match {
    case 256 => 0; // expected-error {{match case can never match a subject of type 'unsigned char'}}
    case _ => 1;
  };
}

int converted_unsigned_integer_value(unsigned value) {
  return value match {
    case -1 => 0;
    case ~0u => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

using U2 = unsigned _BitInt(2);

int missing_small_integer(U2 value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: 2}}
    case 0 => 0;
    case 1 => 1;
    case 3 => 3;
  };
}

int exhaustive_small_integer(U2 value) {
  return value match {
    case 0 => 0;
    case 1 => 1;
    case 2 => 2;
    case 3 => 3;
  };
}

int redundant_small_integer_catch_all(U2 value) {
  return value match {
    case 0 => 0;
    case 1 => 1;
    case 2 => 2;
    case 3 => 3;
    case _ => 4; // expected-error {{match case is redundant}}
  };
}

int impossible_small_integer_value(U2 value) {
  return value match {
    case 4 => 4; // expected-error {{match case can never match a subject of type 'U2' (aka 'unsigned _BitInt(2)')}}
    case _ => 0;
  };
}

using S2 = _BitInt(2);

int exhaustive_signed_small_integer(S2 value) {
  return value match {
    case -2 => -2;
    case -1 => -1;
    case 0 => 0;
    case 1 => 1;
    case _ => 2; // expected-error {{match case is redundant}}
  };
}

int impossible_bool_value(bool value) {
  return value match {
    case 2 => 0; // expected-error {{match case can never match a subject of type 'bool'}}
    case _ => 1;
  };
}

struct IntegerWrapper {
  int value;
};

int exhaustive_exact_declaration(IntegerWrapper value) {
  return value match {
    case IntegerWrapper copy => copy.value;
  };
}

struct Shape {
  virtual ~Shape();
};

struct Circle : Shape {};

int cast_declaration_is_not_exhaustive(Shape &shape) {
  return shape match { // expected-error {{match expression is not exhaustive; example of a missing case: _}}
    case Circle &circle => 0;
  };
}

int exhaustive_cast_declaration(Shape &shape) {
  return shape match {
    case Circle &circle => 0;
    case _ => 1;
  };
}

enum E { A, B, C };

int missing_enum(E e) {
  return e match { // expected-error {{match expression is not exhaustive; example of a missing case: C}}
    case A => 0;
    case B => 1;
  };
}

int exhaustive_enum(E e) {
  return e match {
    case A => 0;
    case B => 1;
    case C => 2;
  };
}

enum MaybeUnusedEnumerator {
  Present,
  StillRequired [[maybe_unused]],
};

int maybe_unused_enumerator_is_required(MaybeUnusedEnumerator value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: StillRequired}}
    case Present => 0;
  };
}

int maybe_unused_enumerator_can_be_covered(MaybeUnusedEnumerator value) {
  return value match {
    case Present => 0;
    case StillRequired => 1;
  };
}

int wildcard_after_enumerators_covers_residual_values(E e) {
  return e match {
    case A => 0;
    case B => 1;
    case C => 2;
    case _ => 3;
  };
}

int residual_value_is_useful_after_enumerators(E e) {
  return e match {
    case A => 0;
    case B => 1;
    case C => 2;
    case static_cast<E>(3) => 3;
  };
}

int explicit_residual_value_completes_enum_domain(E e) {
  return e match {
    case A => 0;
    case B => 1;
    case C => 2;
    case static_cast<E>(3) => 3;
    case _ => 4; // expected-error {{match case is redundant}}
  };
}

int out_of_range_enum_value_is_impossible(E e) {
  return e match {
    case static_cast<E>(4) => 0; // expected-error {{match case can never match a subject of type 'E'}}
    case _ => 4;
  };
}

int duplicate_residual_value(E e) {
  return e match {
    case static_cast<E>(3) => 0;
    case static_cast<E>(3) => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

int wildcard_covers_enumerators_and_residual_values(E e) {
  return e match {
    case A => 0;
    case B => 1;
    case _ => 2;
    case C => 3; // expected-error {{match case is redundant}}
    case static_cast<E>(3) => 4; // expected-error {{match case is redundant}}
  };
}

int redundant_enum(E e) {
  return e match {
    case A => 0;
    case A => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

enum class Aliased : unsigned { First = 1, AlsoFirst = 1, Second = 2 };

int exhaustive_enum_aliases(Aliased value) {
  return value match {
    case Aliased::First => 0;
    case Aliased::Second => 1;
  };
}

int redundant_enum_alias(Aliased value) {
  return value match {
    case Aliased::First => 0;
    case Aliased::AlsoFirst => 1; // expected-error {{match case is redundant}}
    case Aliased::Second => 2;
  };
}

enum Gapless { GaplessA, GaplessB };

int gapless_enum_has_no_residual_values(Gapless value) {
  return value match {
    case GaplessA => 0;
    case GaplessB => 1;
    case _ => 2; // expected-error {{match case is redundant}}
  };
}

enum Gapped { GappedC = 0, GappedD = 2 };

int explicit_values_cover_gapped_enum_domain(Gapped value) {
  return value match {
    case GappedC => 0;
    case 1 => 1;
    case 2 => 2;
    case 3 => 3;
    case _ => 4; // expected-error {{match case is redundant}}
  };
}

int integer_value_covers_named_enumerator(Gapped value) {
  return value match {
    case 2 => 0;
    case GappedD => 1; // expected-error {{match case is redundant}}
    case _ => 2;
  };
}

enum class Fixed : unsigned char { First, Second };

int fixed_enum_retains_residual_values(Fixed value) {
  return value match {
    case Fixed::First => 0;
    case Fixed::Second => 1;
    case _ => 2;
  };
}

enum Signed { Negative = -1, Zero = 0, Positive = 1 };

int explicit_value_completes_signed_enum_domain(Signed value) {
  return value match {
    case Negative => -1;
    case Zero => 0;
    case Positive => 1;
    case static_cast<Signed>(-2) => -2;
    case _ => 2; // expected-error {{match case is redundant}}
  };
}

namespace std {
template<class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};

template<class Provider>
struct alternative_name {
  using provider = Provider;
  __SIZE_TYPE__ index;
  consteval alternative_name(__SIZE_TYPE__ index) : index(index) {}
};

template<>
struct alternative_traits<int*> {
  static constexpr alternative_info alternatives[] = {
    ^^double
  };

  static constexpr __SIZE_TYPE__ index(int*) noexcept { return 0; }

  template<__SIZE_TYPE__, class Self>
  static constexpr double get(Self&&) { return 0; }
};
}

int exhaustive_pointer(int *pointer) {
  return pointer match {
    case {} => 0;
    case { int &value } => value;
  };
}

int pointer_wildcard_after_both_states(int *pointer) {
  return pointer match {
    case {} => 0;
    case { int &value } => value;
    case _ => 2; // expected-error {{match case is redundant}}
  };
}

int missing_null_pointer_state(int *pointer) {
  return pointer match { // expected-error {{match expression is not exhaustive; example of a missing case: {}}}
    case { int &value } => value;
  };
}

int named_pointer_states_are_builtin(int *pointer) {
  return pointer match {
    case { .some: int &value } => value;
    case { .none } => 0;
  };
}

int null_pointer_constant_covers_empty_state(int *pointer) {
  return pointer match {
    case nullptr => 0;
    case { int &value } => value;
  };
}

int empty_pointer_state_redundant_after_nullptr(int *pointer) {
  return pointer match {
    case nullptr => 0;
    case {} => 1; // expected-error {{match case is redundant}}
    case { int &value } => value;
  };
}

int exhaustive_void_pointer(void *pointer) {
  return pointer match {
    case {} => 0;
    case { void } => 1;
  };
}

int null_void_pointer_constant_covers_empty_state(void *pointer) {
  return pointer match {
    case nullptr => 0;
    case { void } => 1;
  };
}

struct FiniteChoice {
  unsigned state;
  friend constexpr bool operator==(FiniteChoice, FiniteChoice) = default;
};

inline constexpr FiniteChoice FiniteFirst{0};
inline constexpr FiniteChoice FiniteFirstAlias{0};
inline constexpr FiniteChoice FiniteSecond{1};
inline constexpr FiniteChoice FiniteThird{2};

template<>
struct std::alternative_traits<FiniteChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^FiniteFirst,
    ^^FiniteSecond,
    ^^FiniteThird
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(FiniteChoice value) noexcept {
    return value.state;
  }

};

int exhaustive_singleton_states(FiniteChoice value) {
  return value match {
    case FiniteFirst => 0;
    case FiniteSecond => 1;
    case FiniteThird => 2;
  };
}

template<class T>
int dependent_singleton_states(T value) {
  return value match {
    case FiniteFirst => 0;
    case FiniteSecond => 1;
    case FiniteThird => 2;
  };
}

int instantiate_dependent_singleton_states(FiniteChoice value) {
  return dependent_singleton_states(value);
}

int missing_singleton_state(FiniteChoice value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: FiniteThird}}
    case FiniteFirst => 0;
    case FiniteSecond => 1;
  };
}

int singleton_alias_is_redundant(FiniteChoice value) {
  return value match {
    case FiniteFirst => 0;
    case FiniteFirstAlias => 1; // expected-error {{match case is redundant}}
    case FiniteSecond => 2;
    case FiniteThird => 3;
  };
}

int singleton_states_are_not_empty(FiniteChoice value) {
  return value match {
    case {} => 0; // expected-error {{type 'FiniteChoice' has no non-projectable alternative state}}
    case _ => 1;
  };
}

struct ClassifiedChoice {
  unsigned state;
  friend constexpr bool operator==(ClassifiedChoice,
                                   ClassifiedChoice) = default;
};

inline constexpr ClassifiedChoice ClassifiedFirst{0};
inline constexpr ClassifiedChoice ClassifiedSecond{1};

template<>
struct std::alternative_traits<ClassifiedChoice> {
  static constexpr alternative_info alternatives[] = {
    {}, {}
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(ClassifiedChoice value) noexcept {
    return value.state;
  }

};

int unrepresented_values_do_not_contribute_to_exhaustiveness(
    ClassifiedChoice value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: _}}
    case ClassifiedFirst => 0;
    case ClassifiedSecond => 1;
  };
}

int index_selectors_cover_unrepresented_states(ClassifiedChoice value) {
  return value match {
    case { .[0] } => 0;
    case { .[1] } => 1;
  };
}

struct FiniteChoiceAndBool {
  FiniteChoice choice;
  bool flag;
};

int singleton_state_coverage_is_recursive(FiniteChoiceAndBool value) {
  return value match {
    case [FiniteFirst, _] => 0;
    case [FiniteSecond, _] => 1;
    case [FiniteThird, false] => 2;
    case [FiniteThird, true] => 3;
  };
}

struct ResidualFiniteChoice {
  unsigned state;
  friend constexpr bool operator==(ResidualFiniteChoice,
                                   ResidualFiniteChoice) = default;
};

inline constexpr ResidualFiniteChoice ResidualFirst{0};
inline constexpr ResidualFiniteChoice ResidualSecond{1};

template<>
struct std::alternative_traits<ResidualFiniteChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^ResidualFirst,
    ^^ResidualSecond
  };
  static constexpr bool has_residual_states = true;

  static constexpr __SIZE_TYPE__ index(ResidualFiniteChoice value) noexcept {
    return value.state < 2 ? value.state : 2;
  }
};

int singleton_required_domain_is_exhaustive(ResidualFiniteChoice value) {
  return value match {
    case ResidualFirst => 0;
    case ResidualSecond => 1;
  };
}

int singleton_residual_state_is_useful(ResidualFiniteChoice value) {
  return value match {
    case ResidualFirst => 0;
    case ResidualSecond => 1;
    case _ => 2;
  };
}

struct InaccessibleChoice {
  unsigned state;
  int payload;
};

template<>
struct std::alternative_traits<InaccessibleChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, {}
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(InaccessibleChoice value) noexcept {
    return value.state;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& value) {
    return (static_cast<Self&&>(value).payload);
  }

};

int inaccessible_alternative_state_can_fall_back(InaccessibleChoice value) {
  return value match {
    case { int payload } => payload;
    case _ => 1;
  };
}

struct NamedOnlyChoice {
  unsigned state;
};

template<>
struct std::alternative_traits<NamedOnlyChoice> {
  using AT = alternative_traits;
  static constexpr alternative_info alternatives[] = {
    {}, {}
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(NamedOnlyChoice value) noexcept {
    return value.state;
  }

  struct names {
    static constexpr alternative_name<AT> first = 0, second = 1;
  };
};

int named_only_alternative_states_are_accessible(NamedOnlyChoice value) {
  return value match {
    case { .first } => 0;
    case { .second } => 1;
  };
}

struct Pair {
  bool x;
  bool y;
};

struct SmallIntegerAndBool {
  U2 i;
  bool b;
};

struct EnumAndBool {
  E e;
  bool b;
};

struct GappedAndBool {
  Gapped e;
  bool b;
};

int required_enum_domain_is_recursive(EnumAndBool value) {
  return value match {
    case [A, _] => 0;
    case [B, _] => 1;
    case [C, _] => 2;
  };
}

int residual_enum_domain_is_recursive(EnumAndBool value) {
  return value match {
    case [A, _] => 0;
    case [B, _] => 1;
    case [C, _] => 2;
    case [_, true] => 3;
    case [_, false] => 4;
  };
}

int explicit_enum_domain_coverage_is_recursive(GappedAndBool value) {
  return value match {
    case [GappedC, _] => 0;
    case [1, false] => 1;
    case [1, true] => 2;
    case [2, _] => 3;
    case [3, _] => 4;
    case _ => 5; // expected-error {{match case is redundant}}
  };
}

int integer_domain_coverage_is_recursive(SmallIntegerAndBool value) {
  return value match {
    case [0, _] => 0;
    case [1, _] => 1;
    case [2, false] => 2;
    case [2, true] => 3;
    case [3, _] => 4;
    case _ => 5; // expected-error {{match case is redundant}}
  };
}

int missing_decomposition(Pair p) {
  return p match { // expected-error {{match expression is not exhaustive; example of a missing case: [false, false]}}
    case [true, _] => 1;
    case [_, true] => 2;
  };
}

int exhaustive_decomposition(Pair p) {
  return p match {
    case [true, _] => 1;
    case [false, _] => 2;
  };
}

namespace std {
class type_info {
public:
  bool operator==(const type_info&) const;
};

}

struct Choice {
  unsigned state;
  bool flag;
  int number;
};

template<>
struct std::alternative_traits<Choice> {
  using AT = alternative_traits;
  static constexpr alternative_info alternatives[] = {
    ^^bool, ^^int, {^^EmptyState::first, true},
    {^^EmptyState::second, true}
  };
  static constexpr bool has_residual_states = false;

  struct names {
    static constexpr alternative_name<AT> flag = 0, number = 1;
  };

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I < 2)
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).flag);
    else
      return (static_cast<Self&&>(choice).number);
  }
};

int exhaustive_alternatives(Choice choice) {
  return choice match {
    case { .flag: _ } => 0;
    case { .number: _ } => 1;
    case {} => 2;
  };
}

int missing_empty_alternative(Choice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: {}}}
    case { .flag: _ } => 0;
    case { .number: _ } => 1;
  };
}

int missing_projected_alternative(Choice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: { bool }}}
    case { .number: _ } => 1;
    case {} => 2;
  };
}

int missing_projected_integer_alternative(Choice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: { int }}}
    case { .flag: _ } => 0;
    case {} => 2;
  };
}

struct ChoiceProduct {
  Choice choice;
  bool flag;
};

int missing_projected_alternative_in_product(ChoiceProduct value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: [{ int }, false]}}
    case [{ .flag: _ }, _] => 0;
    case [{}, _] => 1;
  };
}

int missing_projected_alternative_value(Choice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: { false }}}
    case { .flag: true } => 0;
    case { .number: _ } => 1;
    case {} => 2;
  };
}

int exhaustive_nested_alternatives(Choice choice) {
  return choice match {
    case { true } => 0;
    case { false } => 1;
    case { int number } => number;
    case {} => 2;
  };
}

int exhaustive_generic_alternatives(Choice choice) {
  return choice match {
    case { auto&& value } => static_cast<int>(value);
    case {} => 2;
  };
}

int no_residual_state_after_all_alternatives(Choice choice) {
  return choice match {
    case { auto&& value } => static_cast<int>(value);
    case {} => 2;
    case _ => 3; // expected-error {{match case is redundant}}
  };
}

struct NullableChoice {
  bool engaged;
  int value;
};

struct NullableChoiceView {
  static constexpr std::alternative_info alternatives[] = {
    {^^nullable_empty_state, true}, ^^int
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const NullableChoice& choice) noexcept {
    return choice.engaged ? 1 : 0;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 1)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

template<>
struct std::alternative_traits<NullableChoice> {
  using AT = alternative_traits;
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^int
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const NullableChoice& choice) noexcept {
    return choice.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }

  struct names {
    static constexpr alternative_name<AT> value = 0, error = 1;
    static constexpr alternative_name<NullableChoiceView> none = 0, some = 1;
  };
};

int complete_secondary_view_after_partial_primary(NullableChoice choice) {
  return choice match {
    case { .value: 0 } => 0;
    case { .some: _ } => 1;
    case { .none } => 2;
  };
}

int partial_views_do_not_combine(NullableChoice choice) {
  return choice match { // expected-error {{match expression is not exhaustive}}
    case { .value: _ } => 0;
    case { .none } => 1;
  };
}

int overlap_between_views_is_maybe_useful(NullableChoice choice) {
  return choice match {
    case { .value: _ } => 0;
    case { .some: _ } => 1;
    case { .none } => 2;
  };
}

int complete_view_makes_other_view_redundant(NullableChoice choice) {
  return choice match {
    case { .some: _ } => 0;
    case { .none } => 1;
    case { .value: _ } => 2; // expected-error {{match case is redundant}}
  };
}

struct NullableChoiceProduct {
  NullableChoice choice;
  bool flag;
};

int complete_nested_view_makes_other_view_redundant(
    NullableChoiceProduct value) {
  return value match {
    case [{ .some: _ }, _] => 0;
    case [{ .none }, _] => 1;
    case [{ .value: _ }, _] => 2; // expected-error {{match case is redundant}}
  };
}

struct ResidualChoice {
  unsigned state;
  bool flag;
  int number;
};

template<>
struct std::alternative_traits<ResidualChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^bool, ^^int
  };
  static constexpr bool has_residual_states = true;

  static constexpr __SIZE_TYPE__ index(const ResidualChoice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).flag);
    else
      return (static_cast<Self&&>(choice).number);
  }
};

int projected_states_are_required(ResidualChoice choice) {
  return choice match {
    case { bool flag } => static_cast<int>(flag);
    case { int number } => number;
  };
}

int residual_alternative_state_is_useful(ResidualChoice choice) {
  return choice match {
    case { bool flag } => static_cast<int>(flag);
    case { int number } => number;
    case _ => 2;
  };
}

struct ResidualChoiceAndBool {
  ResidualChoice choice;
  bool flag;
};

int residual_alternative_state_is_recursive(ResidualChoiceAndBool value) {
  return value match {
    case [{ bool flag }, _] => static_cast<int>(flag);
    case [{ int number }, _] => number;
    case [_, true] => 2;
    case [_, false] => 3;
  };
}

int unbraced_type_pattern_does_not_project(Choice choice) {
  return choice match {
    case int => 0; // expected-error {{type pattern of type 'int' is not an exact match for subject of type 'Choice'}}
    case _ => 1;
  };
}

int redundant_alternative(Choice choice) {
  return choice match {
    case { .flag: _ } => 0;
    case { .flag: false } => 1; // expected-error {{match case is redundant}}
    case { .number: _ } => 2;
    case {} => 3;
    case {} => 4; // expected-error {{match case is redundant}}
  };
}

int generic_makes_typed_alternative_redundant(Choice choice) {
  return choice match {
    case { auto&& value } => static_cast<int>(value);
    case { .flag: _ } => 1; // expected-error {{match case is redundant}}
    case {} => 2;
  };
}

int type_selectors_are_exhaustive(Choice choice) {
  return choice match {
    case { bool: _ } => 0;
    case { int: _ } => 1;
    case {} => 2;
  };
}

int expression_selectors_are_exhaustive(Choice choice) {
  return choice match {
    case { .[0]: _ } => 0;
    case { .[1]: _ } => 1;
    case {} => 2;
  };
}

int type_selector_makes_index_redundant(Choice choice) {
  return choice match {
    case { bool: _ } => 0;
    case { .[0]: _ } => 1; // expected-error {{match case is redundant}}
    case { int: _ } => 2;
    case {} => 3;
  };
}

struct ChoiceAndBool {
  Choice choice;
  bool flag;
};

int exhaustive_nested_projection(ChoiceAndBool value) {
  return value match {
    case [{ .flag: _ }, _] => 0;
    case [{ .number: _ }, _] => 1;
    case [{}, _] => 2;
  };
}

struct OpenChoice {};

template<>
struct std::alternative_traits<OpenChoice> {
  static bool has_value(const OpenChoice&);

  template<class T, class Self>
  static T* try_cast(Self&&);
};

int exhaustive_open_alternatives(OpenChoice choice) {
  return choice match {
    case { int value } => value;
    case { _ } => 1;
    case {} => 0;
  };
}

int missing_open_alternative(OpenChoice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: { _ }}}
    case { int value } => value;
    case {} => 0;
  };
}

int missing_open_empty_state(OpenChoice choice) {
  return choice match { // expected-error {{match expression is not exhaustive; example of a missing case: {}}}
    case { _ } => 1;
  };
}

int projectable_wildcard_shadows_open_type(OpenChoice choice) {
  return choice match {
    case { _ } => 1;
    case { int value } => value; // expected-error {{match case is redundant}}
    case {} => 0;
  };
}

int open_type_coverage_ignores_cvref(OpenChoice choice) {
  return choice match {
    case { const int& value } => value;
    case { int value } => value; // expected-error {{match case is redundant}}
    case { _ } => 1;
    case {} => 0;
  };
}

struct OpenChoiceAndBool {
  OpenChoice choice;
  bool flag;
};

int exhaustive_nested_open_alternative(OpenChoiceAndBool value) {
  return value match {
    case [{ int number }, _] => number;
    case [{ _ }, true] => 1;
    case [{ _ }, false] => 2;
    case [{}, _] => 0;
  };
}

int missing_nested_open_empty_state(OpenChoiceAndBool value) {
  return value match { // expected-error {{match expression is not exhaustive; example of a missing case: [{}, false]}}
    case [{ _ }, _] => 1;
    case [{}, true] => 0;
  };
}

int whole_wildcard_shadows_open_states(OpenChoice choice) {
  return choice match {
    case _ => 0;
    case { _ } => 1; // expected-error {{match case is redundant}}
    case {} => 2; // expected-error {{match case is redundant}}
  };
}

struct AlwaysOpen {};

template<>
struct std::alternative_traits<AlwaysOpen> {
  template<class T, class Self>
  static T* try_cast(Self&&);
};

int projectable_wildcard_exhausts_nonnullable_open_choice(AlwaysOpen choice) {
  return choice match {
    case { _ } => 1;
  };
}
