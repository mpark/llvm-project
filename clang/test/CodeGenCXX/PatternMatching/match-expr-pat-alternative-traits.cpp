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
// CHECK: switch i64 %match.index
// CHECK-NOT: icmp eq i64
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

// A residual source case becomes the default edge of the discriminator tree.
// CHECK-LABEL: define{{.*}} i32 @_Z22match_value_or_defaultR8MaybeInt
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: i64 0, label %match.decision.success
// CHECK: match.decision.failure:
int match_value_or_default(MaybeInt& value) {
  return match (value) {
    case { int& number } => number;
    case _ => -1;
  };
}

// A built-in pointer has a bool discriminator. Load it using the C++ scalar
// representation rather than its address representation.
// CHECK-LABEL: define{{.*}} i32 @_Z13match_pointerPi
// CHECK: switch i1 %match.index
// CHECK: i1 true, label %match.decision.success
// CHECK: i1 false, label %match.decision.success
int match_pointer(int* pointer) {
  return match (pointer) {
    case { int& number } => number;
    case {} => -1;
  };
}

// Of the two tests in the first row, the second pointer is tested by more
// rows. It therefore supplies the root discriminator.
// CHECK-LABEL: define{{.*}} i32 @_Z18match_pointer_pairPiS_(
// CHECK: switch i1 %match.index
// CHECK: i1 true, label %match.decision.success
// CHECK: i1 false, label %match.decision.success
// CHECK: match.decision.success:
// CHECK: switch i1 %match.index{{[0-9]+}}
int match_pointer_pair(int* first, int* second) {
  return match (first, second) {
    case [{ int& left }, { int& right }] => left + right;
    case [_, {}] => -1;
    case _ => -2;
  };
}

struct MaybeMaybeInt {
  bool engaged;
  MaybeInt value;
};

template<>
struct std::alternative_traits<MaybeMaybeInt> {
  static constexpr alternative_info alternatives[] = {
    ^^MaybeInt, {^^empty_state, true}
  };
  static constexpr bool has_residual_states = false;

  static __SIZE_TYPE__ index(const MaybeMaybeInt&) noexcept;

  template<__SIZE_TYPE__ I>
    requires(I == 0)
  static MaybeInt& get(MaybeMaybeInt&);
};

// A successful outer constructor exposes its payload as a new decision
// column. Each discriminator is evaluated once, and the inner discriminator
// remains conditional on the outer value state.
// CHECK-LABEL: define{{.*}} i32 @_Z24match_nested_alternativeR13MaybeMaybeInt
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI13MaybeMaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI13MaybeMaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: ret i32
int match_nested_alternative(MaybeMaybeInt& value) {
  return match (value) {
    case {{ int& number }} => number;
    case {{}} => -2;
    case {} => -1;
  };
}

struct MaybeIntPair {
  MaybeInt first;
  MaybeInt second;
};

// Projection paths may pass through a decomposition before exposing more
// closed-alternative columns.
// CHECK-LABEL: define{{.*}} i32 @_Z26match_nested_decompositionP12MaybeIntPair
// CHECK: switch i1 %match.index
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: ret i32
int match_nested_decomposition(MaybeIntPair* value) {
  return match (value) {
    case { [{ int& first }, { int& second }] } => first + second;
    case _ => -1;
  };
}

// A nested discriminator can be materialized independently in disjoint
// subtrees.
// CHECK-LABEL: define{{.*}} i32 @_Z29match_duplicated_nested_pathsR13MaybeMaybeIntS0_
// CHECK-COUNT-4: switch i64 %match.index
// CHECK: ret i32
int match_duplicated_nested_paths(MaybeMaybeInt& first,
                                  MaybeMaybeInt& second) {
  return match (first, second) {
    case [_, {{ int& value }}] => value;
    case [{{ int& value }}, _] => value;
    case _ => -1;
  };
}

// A discriminator may disappear from a row and reappear later. The recursive
// tree returns to the first pointer after testing the second one.
// CHECK-LABEL: define{{.*}} i32 @_Z29match_nonconsecutive_pointersPiS_(
// CHECK: switch i1 %match.index
// CHECK: switch i1 %match.index{{[0-9]+}}
// CHECK: switch i1 %match.index{{[0-9]+}}
int match_nonconsecutive_pointers(int* first, int* second) {
  return match (first, second) {
    case [{ int& }, _] => 1;
    case [_, { int& }] => 2;
    case [{}, _] => 3;
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

struct SwitchChoice {
  unsigned state;
  int first;
  double second;
};

template<>
struct std::alternative_traits<SwitchChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  static __SIZE_TYPE__ index(const SwitchChoice&) noexcept;

  template<__SIZE_TYPE__ I>
  static typename ChoiceAlternative<I>::type& get(SwitchChoice&);
};

// Alternative-dependent instantiations of one source case participate in the
// same decision tree.
// CHECK-LABEL: define{{.*}} i32 @_Z23match_instantiated_caseR12SwitchChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI12SwitchChoiceE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI12SwitchChoiceE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: i64 0, label %match.decision.success
// CHECK: i64 1, label %match.decision.success
// CHECK: ret i32
int match_instantiated_case(SwitchChoice& value) {
  return match (value) {
    case { auto&& alternative } => static_cast<int>(alternative);
  };
}

bool accept_instantiated();

// A failed guard skips the other instantiations of its source case.
// CHECK-LABEL: define{{.*}} i32 @_Z24match_instantiated_guardR12SwitchChoice
// CHECK: switch i64 %match.index
// CHECK: call{{.*}} @_Z19accept_instantiatedv
// CHECK: br i1 {{.*}}, label %match.select.action, label %match.select.guard_failed
// CHECK: match.select.guard_failed:
// CHECK: match.decision.next_pattern:
// CHECK-NOT: alternative_traitsI12SwitchChoiceE3getILm1E
// CHECK: store i32 -1
int match_instantiated_guard(SwitchChoice& value) {
  return match (value) {
    case { auto&& alternative } if (accept_instantiated()) =>
        static_cast<int>(alternative);
    case _ => -1;
  };
}

bool accept_choice();

// Record-valued discriminators may occur in disjoint branches of the decision
// tree. Their condition storage must dominate every use.
// CHECK-LABEL: define{{.*}} i32 @_Z28match_nonconsecutive_choicesR6ChoiceS0_(
// CHECK-COUNT-5: icmp eq i64
int match_nonconsecutive_choices(Choice& first, Choice& second) {
  return match (first, second) {
    case [{ int& }, _] if (accept_choice()) => 1;
    case [_, { int& }] if (accept_choice()) => 2;
    case [{ double& }, _] => 3;
    case _ => 4;
  };
}

struct ChoiceBase {};
struct ChoiceLeft : ChoiceBase {};
struct ChoiceRight : ChoiceBase {};

struct HierarchyChoice {};

template<__SIZE_TYPE__ I>
struct HierarchyAlternative;

template<>
struct HierarchyAlternative<0> {
  using type = ChoiceLeft;
};

template<>
struct HierarchyAlternative<1> {
  using type = ChoiceRight;
};

template<>
struct std::alternative_traits<HierarchyChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^ChoiceLeft, ^^ChoiceRight
  };
  static constexpr bool has_residual_states = false;

  static __SIZE_TYPE__ index(const HierarchyChoice&) noexcept;

  template<__SIZE_TYPE__ I>
  static typename HierarchyAlternative<I>::type& get(HierarchyChoice&);
};

void consume_base(const ChoiceBase&);
void consume_any_base();
bool prefer_whole(const HierarchyChoice&);
void consume_whole(const HierarchyChoice&);

// All projections that initialize the same reference declaration converge on
// one initialization and one copy of the source handler.
// CHECK-LABEL: define{{.*}} void @_Z17match_shared_baseR15HierarchyChoice
// CHECK: switch i64 %match.index
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI15HierarchyChoiceE3getILm0E
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI15HierarchyChoiceE3getILm1E
// CHECK: match.select.shared_action:
// CHECK: phi ptr
// CHECK: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK-NOT: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK: ret void
void match_shared_base(HierarchyChoice& value) {
  match (value) {
    case { const ChoiceBase& base } => consume_base(base);
  }
}

// Declaration-free patterns also converge on one copy of the source handler.
// CHECK-LABEL: define{{.*}} void @_Z22match_shared_base_typeR15HierarchyChoice
// CHECK: switch i64 %match.index
// CHECK: match.select.shared_action:
// CHECK: call{{.*}} @_Z16consume_any_basev
// CHECK-NOT: call{{.*}} @_Z16consume_any_basev
// CHECK: ret void
void match_shared_base_type(HierarchyChoice& value) {
  match (value) {
    case { const ChoiceBase& } => consume_any_base();
  }
}

// A leading ordinary candidate is emitted before the closed-alternative
// failure subtree. Its successful guard never evaluates the discriminator.
// CHECK-LABEL: define{{.*}} void @{{.*}}match_guarded_then_shared_base
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI15HierarchyChoiceE5index
// CHECK: call{{.*}} @{{.*}}prefer_whole
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI15HierarchyChoiceE5index
// CHECK: switch i64 %match.index
// CHECK: match.select.shared_action:
// CHECK: phi ptr
// CHECK: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK-NOT: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK: ret void
void match_guarded_then_shared_base(HierarchyChoice& value) {
  match (value) {
    case HierarchyChoice& whole if (prefer_whole(whole)) =>
      consume_whole(whole);
    case { const ChoiceBase& base } => consume_base(base);
  }
}

// An irrefutable first conjunct exposes the closed alternative in the second
// conjunct to the decision tree while sharing the expanded source handler.
// CHECK-LABEL: define{{.*}} void @{{.*}}match_residual_shared_base
// CHECK: switch
// CHECK: match.select.shared_action:
// CHECK: phi ptr
// CHECK: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK-NOT: call{{.*}} @_Z12consume_baseRK10ChoiceBase
// CHECK: ret void
void match_residual_shared_base(HierarchyChoice& value) {
  match (value) {
    case _ and { const ChoiceBase& base } => consume_base(base);
  }
}

// A declaration in a non-final conjunct is initialized before the following
// conjunct, so it remains an ordered residual pattern.
// CHECK-LABEL: define{{.*}} void @{{.*}}match_sequenced_and
// CHECK-NOT: switch
// CHECK: ret void
void match_sequenced_and(HierarchyChoice& value) {
  match (value) {
    case HierarchyChoice& whole and { const ChoiceBase& base } =>
      consume_base(base);
  }
}

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
  static bool has_value(const OpenChoice&);

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
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE9has_valueERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE9has_valueERKS0_
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

// Sibling discriminators are initialized only when their branch first needs
// them. Each runtime path evaluates both once; the second initialization
// appears separately in the two outer branches.
// CHECK-LABEL: define{{.*}} i32 @_Z20match_choice_productR13ChoiceProduct
// CHECK-COUNT-3: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
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
// CHECK: switch i32 %match.index
// CHECK-NOT: icmp eq
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI13LabeledChoiceE5indexE
// CHECK: ret i32
int match_labeled_value(LabeledChoice value) {
  return match (value) {
    case first_label => 1;
    case second_label => 2;
  };
}
