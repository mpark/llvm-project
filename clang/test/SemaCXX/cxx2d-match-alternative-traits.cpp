// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

static_assert(__has_feature(pattern_matching));
static_assert(__has_feature(reflection));

inline constexpr int empty_state = 0;

namespace std {
class type_info {
public:
  bool operator==(const type_info&) const;
};

template<class T>
struct alternative_traits; // expected-note {{template is declared here}}

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};

}

struct IndexedState {
  __SIZE_TYPE__ value;

  template<__SIZE_TYPE__ I>
  static constexpr __SIZE_TYPE__ index = I;

  constexpr operator __SIZE_TYPE__() const noexcept { return value; }
  friend constexpr bool operator==(IndexedState, IndexedState) = default;
};

struct Choice {
  unsigned state;
  int integer;
  double real;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  struct names {
    __SIZE_TYPE__ value;

    static constexpr __SIZE_TYPE__ integer = 0;
    static constexpr __SIZE_TYPE__ real = 1;
    static constexpr __SIZE_TYPE__ out_of_range = 2;

    template<__SIZE_TYPE__ I>
    static constexpr __SIZE_TYPE__ index = I;

    template<__SIZE_TYPE__ I>
    static constexpr __SIZE_TYPE__ alias = I % 2;

    constexpr operator __SIZE_TYPE__() const noexcept { return value; }
    friend constexpr bool operator==(names, names) = default;
  };

  static constexpr names index(const Choice& choice) noexcept {
    return {choice.state};
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == names::integer)
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
  static constexpr alternative_info alternatives[] = {
    ^^ProjectedNonCopyable, {^^empty_state, true}
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const CopyChoice& choice) noexcept {
    return choice.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

struct SingleChoice {
  int integer;
};

template<>
struct std::alternative_traits<SingleChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^int
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const SingleChoice&) noexcept {
    return 0;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).integer);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
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

struct ThrowingIndex {
  int value;
};

template<>
struct std::alternative_traits<ThrowingIndex> {
  static constexpr alternative_info alternatives[] = {
    ^^int
  };
  static constexpr bool has_residual_states = false;
  enum class names : __SIZE_TYPE__ { value = 0 };
  static names index(const ThrowingIndex&);

  template<names I>
    requires (I == names::value)
  static int& get(ThrowingIndex& choice) {
    return choice.value;
  }
};

bool throwing_index(ThrowingIndex choice) {
  return match(choice, case { .value: int value }); // expected-error {{invalid alternative protocol; 'std::alternative_traits<'ThrowingIndex'>::index' must be noexcept}}
}

struct MissingResidualStates {
  int value;
};

template<>
struct std::alternative_traits<MissingResidualStates> {
  static constexpr alternative_info alternatives[] = {
    ^^int
  };

  static constexpr __SIZE_TYPE__
  index(const MissingResidualStates&) noexcept {
    return 0;
  }

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  static int& get(MissingResidualStates& choice) {
    return choice.value;
  }
};

int missing_residual_states(MissingResidualStates choice) {
  return match (choice) {
    case { int value } => value; // expected-error {{does not provide a usable 'has_residual_states' member}}
  };
}

struct NonArrayDescriptors {};

template<>
struct std::alternative_traits<NonArrayDescriptors> {
  static constexpr alternative_info alternatives = {};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(NonArrayDescriptors) noexcept { return 0; }
};

int non_array_descriptors(NonArrayDescriptors value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{'std::alternative_traits<'NonArrayDescriptors'>::alternatives' must be a constant array of 'std::alternative_info'}}
  };
}

struct WrongDescriptorType {};

template<>
struct std::alternative_traits<WrongDescriptorType> {
  static constexpr int alternatives[] = {0};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(WrongDescriptorType) noexcept { return 0; }
};

int wrong_descriptor_type(WrongDescriptorType value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{'std::alternative_traits<'WrongDescriptorType'>::alternatives' must be a constant array of 'std::alternative_info'}}
  };
}

namespace NotASelector {}
struct InvalidSelectorDescriptor {};

template<>
struct std::alternative_traits<InvalidSelectorDescriptor> {
  static constexpr alternative_info alternatives[] = {
    ^^NotASelector
  };
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(InvalidSelectorDescriptor) noexcept {
    return 0;
  }
};

int invalid_selector_descriptor(InvalidSelectorDescriptor value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{state 0 of type 'InvalidSelectorDescriptor' has an invalid reflection in its 'alternative_info' descriptor}}
  };
}

struct EmptyTypedChoice {};

template<>
struct std::alternative_traits<EmptyTypedChoice> {
  static constexpr alternative_info alternatives[] = {
    {^^int, true}
  };
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(EmptyTypedChoice) noexcept { return 0; }
};

int empty_state_cannot_be_typed(EmptyTypedChoice value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{state 0 of type 'EmptyTypedChoice' is both empty and typed}}
  };
}

struct EmptyProjectedChoice {
  int value;
};

template<>
struct std::alternative_traits<EmptyProjectedChoice> {
  static constexpr alternative_info alternatives[] = {
    {^^empty_state, true}
  };
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(EmptyProjectedChoice) noexcept { return 0; }

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  static constexpr int& get(EmptyProjectedChoice& choice) {
    return choice.value;
  }
};

int empty_state_cannot_be_projected(EmptyProjectedChoice value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{state 0 of type 'EmptyProjectedChoice' is both empty and projectable}}
  };
}

struct EmptyWithoutValueChoice {};

template<>
struct std::alternative_traits<EmptyWithoutValueChoice> {
  static constexpr alternative_info alternatives[] = {{{}, true}};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(EmptyWithoutValueChoice) noexcept { return 0; }
};

int empty_state_requires_value(EmptyWithoutValueChoice value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{empty state 0 of type 'EmptyWithoutValueChoice' is not represented by a value}}
  };
}

inline constexpr int ProjectedSingleton = 0;
struct SingletonProjectedChoice {
  int value;
};

template<>
struct std::alternative_traits<SingletonProjectedChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^ProjectedSingleton
  };
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(SingletonProjectedChoice) noexcept {
    return 0;
  }

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  static constexpr int& get(SingletonProjectedChoice& choice) {
    return choice.value;
  }
};

int singleton_state_cannot_be_projected(SingletonProjectedChoice value) {
  return match (value) {
    case { .index<0> } => 0; // expected-error {{state 0 of type 'SingletonProjectedChoice' is both represented by a value and projectable}}
  };
}

int named(Choice choice) {
  return match (choice) {
    case { .integer: int value } => value;
    case { .real: double value } => static_cast<int>(value);
  };
}

struct GenericSelectorChoice {
  bool error;
  int value;
  long error_value;
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

int generic_selector_default_projection(GenericSelectorChoice choice) {
  return match (choice) {
    case { int value } => value;
    case { long error } => static_cast<int>(error);
  };
}

int generic_selector_named_projection(GenericSelectorChoice choice) {
  return match (choice) {
    case { .value: int value } => value;
    case { .error: long error } => static_cast<int>(error);
  };
}

struct ReflectedOnlyChoice {
  int value;
};

template<>
struct std::alternative_traits<ReflectedOnlyChoice> {
  static constexpr alternative_info alternatives[] = {{}};
  static constexpr bool has_residual_states = false;

  enum class names : __SIZE_TYPE__ { value = 0 };

  static constexpr names index(ReflectedOnlyChoice) noexcept {
    return names::value;
  }

  template<decltype(^^int) Selector>
  static constexpr int& get(ReflectedOnlyChoice& choice) {
    return choice.value;
  }
};

int reflected_get_is_not_a_projection(ReflectedOnlyChoice choice) {
  return match (choice) {
    case { .value: int value } => value; // expected-error {{alternative state 0 of type 'ReflectedOnlyChoice' has no projected value}}
    case _ => 0;
  };
}

int generic_binding(Choice choice) {
  return match (choice) {
    case { int value } => value;
    case { double value } => static_cast<int>(value);
  };
}

int type_selectors(Choice choice) {
  return match (choice) {
    case { int: 0 } => 10;
    case { int: int value } => value;
    case { double: auto value } => static_cast<int>(value);
  };
}

template<class T>
concept Integral = __is_integral(T);

template<class T, class U>
concept SameAs = __is_same(T, U);

template<class T>
constexpr bool is_lvalue_reference = false;

template<class T>
constexpr bool is_lvalue_reference<T&> = true;

template<class T>
concept LvalueReference = is_lvalue_reference<T>;

int type_constraint_selectors(Choice choice) {
  return match (choice) {
    case { Integral: auto value } => value;
    case { SameAs<double>: auto value } => static_cast<int>(value);
  };
}

int no_viable_type_constraint_selector(Choice choice) {
  return match (choice) {
    case { SameAs<char>: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case { _ } => 1;
  };
}

int type_constraint_uses_declared_alternative_type(Choice& choice) {
  return match (choice) {
    case { LvalueReference: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case { _ } => 1;
  };
}

int redundant_after_type_constraint_selector(Choice choice) {
  return match (choice) {
    case { Integral: _ } => 0;
    case { int: _ } => 1; // expected-error {{match case is redundant}}
    case { _ } => 2;
  };
}

template<class T>
int dependent_type_constraint_selector(Choice choice) {
  return match (choice) {
    case { SameAs<T>: auto value } => static_cast<int>(value);
    case { _ } => -1;
  };
}

template int dependent_type_constraint_selector<int>(Choice);
template int dependent_type_constraint_selector<double>(Choice);

int parameterized_names(Choice choice) {
  return match (choice) {
    case { .index<0>: int value } => value;
    case { .index<1>: double value } => static_cast<int>(value);
  };
}

int provider_defined_parameterized_name(Choice choice) {
  return match (choice) {
    case { .alias<2>: int value } => value;
    case { .alias<3>: double value } => static_cast<int>(value);
  };
}

int parameterized_name_out_of_range(Choice choice) {
  return match (choice) {
    case { .index<2>: _ } => 0; // expected-error {{alternative index 2 is outside the range [0, 2)}}
    case _ => 1;
  };
}

int parameterized_name_negative(Choice choice) {
  return match (choice) {
    case { .index<-1>: _ } => 0; // expected-error {{alternative index -1 is outside the range [0, 2)}}
    case _ => 1;
  };
}

int parameterized_name_not_constant(Choice choice,
                                     unsigned index) { // expected-note {{declared here}}
  return match (choice) {
    case { .index<index>: _ } => 0; // expected-error {{expression is not an integral constant expression}} expected-note {{function parameter 'index' with unknown value cannot be used in a constant expression}}
    case _ => 1;
  };
}

int parameterized_name_requires_projection(Choice choice) {
  return match (choice) {
    case { .index<1>: _ } => 0;
    case { .index<0>: _ } => 1;
  };
}

struct IndexOnlyChoice {
  unsigned state;
};

template<>
struct std::alternative_traits<IndexOnlyChoice> {
  static constexpr alternative_info alternatives[] = {{}, {}};
  static constexpr bool has_residual_states = false;

  static constexpr IndexedState index(IndexOnlyChoice choice) noexcept {
    return {choice.state};
  }
};

int index_only_states(IndexOnlyChoice choice) {
  return match (choice) {
    case { .index<0> } => 0;
    case { .index<1> } => 1;
  };
}

int missing_index_only_state(IndexOnlyChoice choice) {
  return match (choice) { // expected-error {{match expression is not exhaustive; example of a missing case: { .index<1> }}}
    case { .index<0> } => 0;
  };
}

int index_only_state_cannot_be_projected(IndexOnlyChoice choice) {
  return match (choice) {
    case { .index<0>: int value } => value; // expected-error {{alternative state 0 of type 'IndexOnlyChoice' has no projected value; omit the ': pattern'}}
    case _ => 0;
  };
}

struct AnonymousProjection {
  int value;
};

template<>
struct std::alternative_traits<AnonymousProjection> {
  static constexpr alternative_info alternatives[] = {{}};
  static constexpr bool has_residual_states = false;

  static constexpr IndexedState index(AnonymousProjection) noexcept {
    return {0};
  }

  template<IndexedState State, class Self>
    requires (State.value == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

int anonymous_projection_by_index(AnonymousProjection choice) {
  return match (choice) {
    case { .index<0>: int value } => value;
  };
}

int anonymous_projection_is_generic(AnonymousProjection choice) {
  return match (choice) {
    case { int value } => value;
  };
}

int anonymous_projection_has_no_type_selector(AnonymousProjection choice) {
  return match (choice) {
    case { int: _ } => 1; // expected-error {{braced alternative pattern does not match any projectable state of 'AnonymousProjection'}}
    case _ => 0;
  };
}

struct TypedWithoutProjection {};

template<>
struct std::alternative_traits<TypedWithoutProjection> {
  static constexpr alternative_info alternatives[] = {^^int};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(TypedWithoutProjection) noexcept { return 0; }
};

int advertised_type_requires_projection(TypedWithoutProjection choice) {
  return match (choice) {
    case { int: _ } => 0; // expected-error {{invalid alternative protocol; state 0 of type 'TypedWithoutProjection' advertises type 'int', but 'get<0>' does not provide a compatible projection}}
  };
}

struct IncompatibleProjection {
  double value;
};

template<>
struct std::alternative_traits<IncompatibleProjection> {
  static constexpr alternative_info alternatives[] = {^^int};
  static constexpr bool has_residual_states = false;
  static constexpr IndexedState index(IncompatibleProjection) noexcept {
    return {0};
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

int advertised_type_requires_compatible_projection(IncompatibleProjection choice) {
  return match (choice) {
    case { int: _ } => 0; // expected-error {{invalid alternative protocol; state 0 of type 'IncompatibleProjection' advertises type 'int', but 'get<0>' does not provide a compatible projection}}
  };
}

double index_selector_ignores_advertised_type(IncompatibleProjection choice) {
  return match (choice) {
    case { .index<0>: double value } => value;
  };
}

struct VoidProjection {};

template<>
struct std::alternative_traits<VoidProjection> {
  static constexpr alternative_info alternatives[] = {^^void};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(VoidProjection) noexcept { return 0; }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr void get(Self&&) {}
};

int advertised_void_projection(VoidProjection choice) {
  return match (choice) {
    case { void: _ } => 0;
  };
}

int projected_deleted_copy_does_not_fall_back(const CopyChoice& choice) {
  return match (choice) {
    case { auto copy } => 1; // expected-error {{call to deleted constructor of 'ProjectedNonCopyable'}}
    case _ => 0;
  };
}

int projected_deleted_hypothetical_copy_is_invalid(const CopyChoice& choice) {
  return match (choice) {
    case { ProjectedNonCopyable } => 1; // expected-error {{call to deleted constructor of 'ProjectedNonCopyable'}}
    case _ => 0;
  };
}

int type_selector_does_not_initialize(const CopyChoice& choice) {
  return match (choice) {
    case { ProjectedNonCopyable: _ } => 1;
    case {} => 0;
  };
}

constexpr int direct_constexpr_alternative() {
  if constexpr (case { int value } = Choice{0, 42, 0.0})
    return value;
  else
    return -1;
}

static_assert(direct_constexpr_alternative() == 42);

template <unsigned State>
constexpr int dependent_constexpr_alternative() {
  if constexpr (case { int value } = Choice{State, 42, 1.0}) {
    static_assert(State == 0);
    return value;
  } else {
    static_assert(State != 0);
    return -1;
  }
}

static_assert(dependent_constexpr_alternative<0>() == 42);
static_assert(dependent_constexpr_alternative<1>() == -1);

int empty(MaybeInt value) {
  return match (value) {
    case { int number } => number;
    case {} => -1;
  };
}

int named_selector_requires_enum_discriminator(MaybeInt value) {
  return match (value) {
    case { .value: int number } => number; // expected-error {{alternative name 'value' is not defined by the discriminator type}}
    case _ => 0;
  };
}

int index_selector_requires_parameterized_name(MaybeInt value) {
  return match (value) {
    case { .index<0>: int number } => number; // expected-error {{alternative name 'index' is not defined by the discriminator type}}
    case _ => 0;
  };
}

int bad_name(Choice choice) {
  return match (choice) {
    case { .missing: int value } => value; // expected-error {{alternative name 'missing' is not defined}}
    case _ => 0;
  };
}

int bad_name_type(Choice choice) {
  return match (choice) {
    case { .not_a_name: int value } => value; // expected-error {{alternative name 'not_a_name' is not defined}}
    case _ => 0;
  };
}

int bad_name_index(Choice choice) {
  return match (choice) {
    case { .out_of_range: int value } => value; // expected-error {{alternative name 'out_of_range' is not defined}}
    case _ => 0;
  };
}

int bad_empty(Choice choice) {
  return match (choice) {
    case {} => 0; // expected-error {{type 'Choice' has no non-projectable alternative state}}
    case _ => 1;
  };
}

int classify(int&);
int classify(double&);

int generic(Choice choice) {
  return match (choice) {
    case { auto&& value } => classify(value);
  };
}

int no_viable_alternative(Choice choice) {
  return match (choice) {
    case { char value } => value; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 0;
  };
}

int no_viable_type_selector(Choice choice) {
  return match (choice) {
    case { char: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 1;
  };
}

template<class T, unsigned I>
int dependent_selectors(Choice choice) {
  return match (choice) {
    case { T: auto value } => static_cast<int>(value);
    case { .index<I>: auto value } => static_cast<int>(value);
    case _ => 0;
  };
}

template int dependent_selectors<int, 1>(Choice);

int valid_single_alternative(SingleChoice choice) {
  return match (choice) {
    case { int value } => value;
  };
}

int no_viable_single_alternative(SingleChoice choice) {
  return match (choice) {
    case { char value } => value; // expected-error {{braced alternative pattern does not match any projectable state of 'SingleChoice'}}
    case _ => 0;
  };
}

template<class T>
int dependent_single_alternative(T choice) {
  return match (choice) {
    case { char value } => value;
    case _ => 0;
  };
}

int instantiate_dependent_single_alternative(SingleChoice choice) {
  return dependent_single_alternative(choice);
}

int direct_single_alternative(SingleChoice choice) {
  if (case { auto&& value } = choice)
    return value;
  return 0;
}

template<class T>
concept CanDirectlyProjectChar = requires(T value) {
  match(value, case { char projected });
};

static_assert(!CanDirectlyProjectChar<SingleChoice>);

bool invalid_direct_single_alternative(SingleChoice choice) {
  return match(choice, case { char value }); // expected-error {{braced alternative pattern does not match any projectable state of 'SingleChoice'}}
}

void single_alternative_loop_conditions(SingleChoice choice) {
  while (case { auto&& value } = choice) {
    (void)value;
    break;
  }
  for (; case { auto&& value } = choice; (void)value) {
    break;
  }
}

int no_structural_alternative(Choice choice) {
  return match (choice) {
    case { auto&& [first, second] } => first; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 0;
  };
}

struct ChoicePair {
  Choice first;
  int second;
};

int direct_generic_alternative(ChoicePair pair) {
  if (case [{ auto&& value }, _] = pair)
    return classify(value);
  return 0;
}

bool standalone_generic_alternative(Choice choice) {
  return match(choice, case { auto&& value });
}

template<class C>
bool dependent_standalone_generic_alternative(C choice) {
  return match(choice, case { auto&& value });
}

bool instantiate_dependent_standalone_generic_alternative(Choice choice) {
  return dependent_standalone_generic_alternative(choice);
}

int direct_generic_alternative_case_condition(ChoicePair pair) {
  if (case [{ auto&& value }, _] = pair)
    return classify(value);
  return 0;
}

int direct_generic_guard(ChoicePair pair) {
  if (case [{ auto&& value }, _] = pair) {
    if (classify(value) != 0)
      return classify(value);
  }
  return 0;
}

bool nested_match_subject(ChoicePair pair) {
  return match(match(pair, case [{ auto&& value }, _]), case true);
}

template<class T>
concept CanDirectlyMatchChar = requires(T value) {
  match(value, case [{ char c }, _]);
};

static_assert(!CanDirectlyMatchChar<ChoicePair>);

bool invalid_direct_alternative(ChoicePair pair) {
  return match(pair, case [{ char c }, _]); // expected-error {{braced alternative pattern does not match any projectable state of 'ChoicePair'}}
}

struct NoTraits {};

int missing_traits(NoTraits value) {
  return match (value) {
    case { NoTraits copy } => 0; // expected-error {{implicit instantiation of undefined template 'std::alternative_traits<NoTraits>'}}
    case _ => 1;
  };
}

struct OpenChoice {};

struct OpenNonCopyable {
  OpenNonCopyable(const OpenNonCopyable&) = delete;
};

template<>
struct std::alternative_traits<OpenChoice> {
  static bool has_value(const OpenChoice&);

  template<class T, class Self>
  static T* try_cast(Self&&);
};

int open_alternatives(OpenChoice choice) {
  return match (choice) {
    case { int value } => value;
    case { double } => 2;
    case { _ } => 1;
    case {} => 0;
  };
}

int open_type_selector(OpenChoice choice) {
  return match (choice) {
    case { int: auto value } => value;
    case _ => 0;
  };
}

int open_parameterized_name(OpenChoice choice) {
  return match (choice) {
    case { .index<0>: _ } => 1; // expected-error {{alternative name 'index' is not defined}}
    case _ => 0;
  };
}

int open_type_selector_does_not_initialize(OpenChoice choice) {
  return match (choice) {
    case { OpenNonCopyable: _ } => 1;
    case _ => 0;
  };
}

int open_type_constraint_selector(OpenChoice choice) {
  return match (choice) {
    case { Integral: _ } => 1; // expected-error {{type-constraint alternative selector cannot be used with open alternative type 'OpenChoice'}}
    case { _ } => 0;
  };
}

bool open_empty(OpenChoice choice) {
  return match(choice, case {});
}

template<class T>
int dependent_open_type(OpenChoice choice) {
  return match (choice) {
    case { T value } => static_cast<int>(value);
    case _ => 0;
  };
}

int instantiate_dependent_open_type(OpenChoice choice) {
  return dependent_open_type<int>(choice);
}

int open_requires_type_direction(OpenChoice choice) {
  return match (choice) {
    case { auto&& value } => 1; // expected-error {{open alternative protocol for type 'OpenChoice' requires a declaration or type pattern with a non-placeholder, non-void type}}
    case _ => 0;
  };
}

struct AlwaysOpen {};

template<>
struct std::alternative_traits<AlwaysOpen> {
  template<class T, class Self>
  static T* try_cast(Self&&);
};

int always_open(AlwaysOpen choice) {
  return match (choice) {
    case { _ } => 1;
  };
}

int always_open_has_no_empty_state(AlwaysOpen choice) {
  return match (choice) {
    case {} => 0; // expected-error {{type 'AlwaysOpen' has no non-projectable alternative state}}
    case { _ } => 1;
  };
}

struct ConstOpenChoice {};

template<>
struct std::alternative_traits<ConstOpenChoice> {
  template<class T, class Self>
  static const T* try_cast(Self&&);
};

int mutable_reference_does_not_bind_to_const_projection(
    ConstOpenChoice choice) {
  return match (choice) {
    case { int& value } => value; // expected-error {{declaration pattern of type 'int &' is not an exact match for subject of type 'const int'}}
    case { _ } => 0;
  };
}

struct InvalidOpen {};

template<>
struct std::alternative_traits<InvalidOpen> {
  template<class T, class Self>
  static int try_cast(Self&&);
};

int invalid_open_protocol(InvalidOpen choice) {
  return match (choice) {
    case { int } => 1; // expected-error {{invalid open alternative protocol for type 'InvalidOpen'; 'try_cast' must return a pointer}}
    case { _ } => 0;
  };
}

struct IncompleteOpen {};

template<>
struct std::alternative_traits<IncompleteOpen> {};

int incomplete_open_protocol(IncompleteOpen choice) {
  return match (choice) {
    case { int } => 1; // expected-error {{does not provide a usable either 'alternatives' or 'try_cast' member}}
    case _ => 0;
  };
}
