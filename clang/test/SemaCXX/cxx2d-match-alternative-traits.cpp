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

template<class Provider>
struct alternative_name {
  using provider = Provider;
  __SIZE_TYPE__ index;
  consteval alternative_name(__SIZE_TYPE__ index) : index(index) {}
};
}

struct Choice {
  unsigned state;
  int integer;
  double real;
};

template<>
struct std::alternative_traits<Choice> {
  using AT = alternative_traits;
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  struct names {
    static constexpr alternative_name<AT> integer = 0, real = 1;
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
  using AT = alternative_traits;
  static constexpr alternative_info alternatives[] = {
    ^^int
  };
  static constexpr bool has_residual_states = false;
  struct names {
    static constexpr alternative_name<AT> value = 0;
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
  return choice match {
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
  return value match {
    case { .[0] } => 0; // expected-error {{'std::alternative_traits<'NonArrayDescriptors'>::alternatives' must be a constant array of 'std::alternative_info'}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{'std::alternative_traits<'WrongDescriptorType'>::alternatives' must be a constant array of 'std::alternative_info'}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{state 0 of type 'InvalidSelectorDescriptor' has an invalid reflection in its 'alternative_info' descriptor}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{state 0 of type 'EmptyTypedChoice' is both empty and typed}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{state 0 of type 'EmptyProjectedChoice' is both empty and projectable}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{empty state 0 of type 'EmptyWithoutValueChoice' is not represented by a value}}
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
  return value match {
    case { .[0] } => 0; // expected-error {{state 0 of type 'SingletonProjectedChoice' is both represented by a value and projectable}}
  };
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

int type_selectors(Choice choice) {
  return choice match {
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
  return choice match {
    case { Integral: auto value } => value;
    case { SameAs<double>: auto value } => static_cast<int>(value);
  };
}

int no_viable_type_constraint_selector(Choice choice) {
  return choice match {
    case { SameAs<char>: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case { _ } => 1;
  };
}

int type_constraint_uses_declared_alternative_type(Choice& choice) {
  return choice match {
    case { LvalueReference: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case { _ } => 1;
  };
}

int redundant_after_type_constraint_selector(Choice choice) {
  return choice match {
    case { Integral: _ } => 0;
    case { int: _ } => 1; // expected-error {{match case is redundant}}
    case { _ } => 2;
  };
}

template<class T>
int dependent_type_constraint_selector(Choice choice) {
  return choice match {
    case { SameAs<T>: auto value } => static_cast<int>(value);
    case { _ } => -1;
  };
}

template int dependent_type_constraint_selector<int>(Choice);
template int dependent_type_constraint_selector<double>(Choice);

int expression_selectors(Choice choice) {
  return choice match {
    case { .[0]: int value } => value;
    case { .[1]: double value } => static_cast<int>(value);
  };
}

int expression_selector_out_of_range(Choice choice) {
  return choice match {
    case { .[2]: _ } => 0; // expected-error {{alternative index 2 is outside the range [0, 2)}}
    case _ => 1;
  };
}

int expression_selector_negative(Choice choice) {
  return choice match {
    case { .[-1]: _ } => 0; // expected-error {{alternative index -1 is outside the range [0, 2)}}
    case _ => 1;
  };
}

int expression_selector_not_constant(Choice choice,
                                     unsigned index) { // expected-note {{declared here}}
  return choice match {
    case { .[index]: _ } => 0; // expected-error {{expression is not an integral constant expression}} expected-note {{function parameter 'index' with unknown value cannot be used in a constant expression}}
    case _ => 1;
  };
}

int expression_selector_requires_projection(Choice choice) {
  return choice match {
    case { .[1]: _ } => 0;
    case { .[0]: _ } => 1;
  };
}

struct IndexOnlyChoice {
  unsigned state;
};

template<>
struct std::alternative_traits<IndexOnlyChoice> {
  static constexpr alternative_info alternatives[] = {{}, {}};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(IndexOnlyChoice choice) noexcept {
    return choice.state;
  }
};

int index_only_states(IndexOnlyChoice choice) {
  return choice match {
    case { .[0] } => 0;
    case { .[1] } => 1;
  };
}

int index_only_state_cannot_be_projected(IndexOnlyChoice choice) {
  return choice match {
    case { .[0]: int value } => value; // expected-error {{alternative state 0 of type 'IndexOnlyChoice' has no projected value; omit the ': pattern'}}
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

  static constexpr unsigned index(AnonymousProjection) noexcept { return 0; }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

int anonymous_projection_by_index(AnonymousProjection choice) {
  return choice match {
    case { .[0]: int value } => value;
  };
}

int anonymous_projection_is_generic(AnonymousProjection choice) {
  return choice match {
    case { int value } => value;
  };
}

int anonymous_projection_has_no_type_selector(AnonymousProjection choice) {
  return choice match {
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
  return choice match {
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
  static constexpr unsigned index(IncompatibleProjection) noexcept { return 0; }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& choice) {
    return (static_cast<Self&&>(choice).value);
  }
};

int advertised_type_requires_compatible_projection(IncompatibleProjection choice) {
  return choice match {
    case { int: _ } => 0; // expected-error {{invalid alternative protocol; state 0 of type 'IncompatibleProjection' advertises type 'int', but 'get<0>' does not provide a compatible projection}}
  };
}

double index_selector_ignores_advertised_type(IncompatibleProjection choice) {
  return choice match {
    case { .[0]: double value } => value;
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
  return choice match {
    case { void: _ } => 0;
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

int type_selector_does_not_initialize(const CopyChoice& choice) {
  return choice match {
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

int no_viable_type_selector(Choice choice) {
  return choice match {
    case { char: _ } => 0; // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
    case _ => 1;
  };
}

template<class T, unsigned I>
int dependent_selectors(Choice choice) {
  return choice match {
    case { T: auto value } => static_cast<int>(value);
    case { .[I]: auto value } => static_cast<int>(value);
    case _ => 0;
  };
}

template int dependent_selectors<int, 1>(Choice);

int valid_single_alternative(SingleChoice choice) {
  return choice match {
    case { int value } => value;
  };
}

int no_viable_single_alternative(SingleChoice choice) {
  return choice match {
    case { char value } => value; // expected-error {{braced alternative pattern does not match any projectable state of 'SingleChoice'}}
    case _ => 0;
  };
}

template<class T>
int dependent_single_alternative(T choice) {
  return choice match {
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
  value match case { char projected };
};

static_assert(!CanDirectlyProjectChar<SingleChoice>);

bool invalid_direct_single_alternative(SingleChoice choice) {
  return choice match case { char value }; // expected-error {{braced alternative pattern does not match any projectable state of 'SingleChoice'}}
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
  if (case [{ auto&& value }, _] = pair)
    return classify(value);
  return 0;
}

bool standalone_generic_alternative(Choice choice) {
  return choice match case { auto&& value };
}

template<class C>
bool dependent_standalone_generic_alternative(C choice) {
  return choice match case { auto&& value };
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

struct OpenChoice {};

template<>
struct std::alternative_traits<OpenChoice> {
  static bool has_value(const OpenChoice&);

  template<class T, class Self>
  static T* try_cast(Self&&);
};

int open_alternatives(OpenChoice choice) {
  return choice match {
    case { int value } => value;
    case { double } => 2;
    case { _ } => 1;
    case {} => 0;
  };
}

int open_type_selector(OpenChoice choice) {
  return choice match {
    case { int: auto value } => value;
    case _ => 0;
  };
}

int open_expression_selector(OpenChoice choice) {
  return choice match {
    case { .[0]: _ } => 1; // expected-error {{expression alternative selector cannot be used with open alternative type 'OpenChoice'}}
    case _ => 0;
  };
}

int open_type_constraint_selector(OpenChoice choice) {
  return choice match {
    case { Integral: _ } => 1; // expected-error {{type-constraint alternative selector cannot be used with open alternative type 'OpenChoice'}}
    case { _ } => 0;
  };
}

bool open_empty(OpenChoice choice) {
  return choice match case {};
}

template<class T>
int dependent_open_type(OpenChoice choice) {
  return choice match {
    case { T value } => static_cast<int>(value);
    case _ => 0;
  };
}

int instantiate_dependent_open_type(OpenChoice choice) {
  return dependent_open_type<int>(choice);
}

int open_requires_type_direction(OpenChoice choice) {
  return choice match {
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
  return choice match {
    case { _ } => 1;
  };
}

int always_open_has_no_empty_state(AlwaysOpen choice) {
  return choice match {
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
  return choice match {
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
  return choice match {
    case { int } => 1; // expected-error {{invalid open alternative protocol for type 'InvalidOpen'; 'try_cast' must return a pointer}}
    case { _ } => 0;
  };
}

struct IncompleteOpen {};

template<>
struct std::alternative_traits<IncompleteOpen> {};

int incomplete_open_protocol(IncompleteOpen choice) {
  return choice match {
    case { int } => 1; // expected-error {{does not provide a usable either 'alternatives' or 'try_cast' member}}
    case _ => 0;
  };
}
