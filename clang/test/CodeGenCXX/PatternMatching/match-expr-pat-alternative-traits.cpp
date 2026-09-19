// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

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

struct IndexedState {
  __SIZE_TYPE__ value;

  template<__SIZE_TYPE__ I>
  static constexpr __SIZE_TYPE__ index = I;

  constexpr operator __SIZE_TYPE__() const noexcept { return value; }
  __attribute__((always_inline)) friend constexpr bool
  operator==(IndexedState left, IndexedState right) {
    return left.value == right.value;
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

  static __SIZE_TYPE__ index(const MaybeInt&) noexcept;

  template<__SIZE_TYPE__ I>
    requires(I == 0)
  static int& get(MaybeInt&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z17match_alternativeR8MaybeInt
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0E
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0E
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: ret i32
int match_alternative(MaybeInt& value) {
  return match (value) {
    case { int& number } if (number == 0) => 0;
    case { int& number } => number;
    case {} => -1;
  };
}

// A projection with only one projectable state uses the same semantic
// instantiation model, including when its binding is injected into a loop body.
// CHECK-LABEL: define{{.*}} i32 @_Z23match_alternative_whileR8MaybeInt
int match_alternative_while(MaybeInt& value) {
  int result = -1;
  while (case { int& number } = value) {
    result = number;
    value.engaged = false;
  }
  return result;
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
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  static IndexedState index(const Choice&) noexcept;

  template<__SIZE_TYPE__ I>
  static typename ChoiceAlternative<I>::type& get(Choice&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z19match_selected_typeR6Choice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm0E
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm1E
// CHECK: ret i32
int match_selected_type(Choice& value) {
  return match (value) {
    case { int: auto number } => number;
    case { double: auto number } => static_cast<int>(number);
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z20match_selected_indexR6Choice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm0E
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm1E
// CHECK: ret i32
int match_selected_index(Choice& value) {
  return match (value) {
    case { .index<0>: int number } => number;
    case { .index<1>: double number } => static_cast<int>(number);
  };
}

// A generic projected arm in a dependent declaration context must not be
// specialized until the enclosing template is instantiated. Otherwise,
// references to non-dependent parameters can be cloned as internal globals.
// CHECK-NOT: @_ZZ{{.*}}match_nondependent_subject_in_template{{.*}}5value = internal global
// CHECK-LABEL: define{{.*}} i32 @_Z{{.*}}match_nondependent_subject_in_template
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
template<class = void>
int match_nondependent_subject_in_template(Choice& value) {
  return match (value) {
    case { auto&& alternative } => static_cast<int>(alternative);
  };
}

template int match_nondependent_subject_in_template<>(Choice&);

template<class T>
int match_dependent_alternative(T& value, int& guards) {
  return match (value) {
    case { auto&& alternative } if (++guards == 2) =>
        static_cast<int>(alternative);
    case _ => 0;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z41instantiate_dependent_alternative_patternR6ChoiceRi
// CHECK: call{{.*}} @_Z27match_dependent_alternativeI6ChoiceEiRT_Ri
int instantiate_dependent_alternative_pattern(Choice& value, int& guards) {
  return match_dependent_alternative(value, guards);
}

template<class T>
void match_dependent_statement(T& value, int& result) {
  match (value) {
    case { auto&& alternative } => {
      result = static_cast<int>(alternative);
    }
  }
}

// CHECK-LABEL: define{{.*}} void @_Z25match_dependent_statementI6ChoiceEvRT_Ri
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm0E
// CHECK: store i32
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE3getILm1E
// CHECK: store i32
template void match_dependent_statement(Choice&, int&);

template<class Outer>
int match_nested_dependent_alternative(Choice& value, int& guards) {
  return [&]<class Inner>(Inner) {
    return match (value) {
      case { auto&& alternative }
          if (++guards == 2 && sizeof(Outer) == sizeof(Inner)) =>
              static_cast<int>(alternative);
      case _ => 0;
    };
  }(Outer{});
}

// CHECK-LABEL: define{{.*}} i32 @_Z48instantiate_nested_dependent_alternative_patternR6ChoiceRi
// CHECK: call{{.*}} @_Z34match_nested_dependent_alternativeIiEiR6ChoiceRi
int instantiate_nested_dependent_alternative_pattern(Choice& value,
                                                       int& guards) {
  return match_nested_dependent_alternative<int>(value, guards);
}

struct OpenChoice {};

template<>
struct std::alternative_traits<OpenChoice> {
  static bool empty(const OpenChoice&);

  template<class T, class Self>
  static T* try_cast(Self&&);

  template<class T, class Self>
  static int* try_init(Self&&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z22match_open_alternativeR10OpenChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_cast
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_cast
// CHECK: ret i32
int match_open_alternative(OpenChoice& value) {
  return match (value) {
    case { int: int& number } if (number == 0) => 0;
    case { int: int& number } => number;
    case _ => -2;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z33match_open_initialization_patternR10OpenChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_init
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_init
// CHECK: ret i32
int match_open_initialization_pattern(OpenChoice& value) {
  return match (value) {
    case { int& number } if (number == 0) => 0;
    case { int& number } => number;
    case _ => -2;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z30match_distinct_open_operationsR10OpenChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_cast
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_init
// CHECK: ret i32
int match_distinct_open_operations(OpenChoice& value) {
  return match (value) {
    case { int: _ } if (false) => 0;
    case { int& number } => number;
    case _ => -2;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z22match_open_empty_stateR10OpenChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE5emptyERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE5emptyERKS0_
// CHECK: ret i32
int match_open_empty_state(OpenChoice& value) {
  return match (value) {
    case {} => -1;
    case _ => 0;
  };
}

struct ChoiceProduct {
  Choice first;
  Choice second;
};

int combine(int&, int&);
int combine(int&, double&);
int combine(double&, int&);
int combine(double&, double&);

// Both sibling discriminators are initialized before the first selected-index
// comparison, and neither is recomputed in another Cartesian branch.
// CHECK-LABEL: define{{.*}} i32 @_Z20match_choice_productR13ChoiceProduct
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: icmp eq
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: ret i32
int match_choice_product(ChoiceProduct& value) {
  return match (value) {
    case [{ auto&& first }, { auto&& second }] => combine(first, second);
  };
}

struct StateLabel {
  unsigned value;
};

inline constexpr StateLabel first_label{0};
inline constexpr StateLabel second_label{1};

struct LabeledChoice {
  unsigned state;
};

template<>
struct std::alternative_traits<LabeledChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^first_label, ^^second_label
  };
  static constexpr bool has_residual_states = false;

  static unsigned index(LabeledChoice) noexcept;
};

// Advertised values dispatch through the cached discriminator. They neither
// require equality with the subject nor recompute index for later arms.
// CHECK-LABEL: define{{.*}} i32 @_Z19match_labeled_value13LabeledChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI13LabeledChoiceE5indexE
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI13LabeledChoiceE5indexE
// CHECK: icmp eq
// CHECK: icmp eq
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI13LabeledChoiceE5indexE
// CHECK: ret i32
int match_labeled_value(LabeledChoice value) {
  return match (value) {
    case first_label => 1;
    case second_label => 2;
  };
}
