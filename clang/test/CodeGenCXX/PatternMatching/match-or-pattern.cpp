// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown \
// RUN:   -fpattern-matching -O1 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown \
// RUN:   -fpattern-matching -O0 -emit-llvm %s -o - | \
// RUN:   FileCheck %s --check-prefix=OR-TREE

// CHECK-LABEL: define{{.*}} i32 @_Z8classifyi(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
// CHECK: %[[IN_RANGE:.*]] = icmp ult i32 %[[VALUE]], 2
// CHECK: %[[RESULT:.*]] = zext i1 %[[IN_RANGE]] to i32
// CHECK: ret i32 %[[RESULT]]
int classify(int value) {
  return match (value) {
    case 0 or 1 => 1;
    case _ => 0;
  };
}

extern bool first(int);
extern bool second(int);

struct FirstPattern {};
struct SecondPattern {};

inline constexpr FirstPattern first_pattern;
inline constexpr SecondPattern second_pattern;

bool operator==(int value, FirstPattern) { return first(value); }
bool operator==(int value, SecondPattern) { return second(value); }

// CHECK-LABEL: define{{.*}} i1 @_Z13short_circuiti(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
// CHECK: %[[FIRST:.*]] = tail call{{.*}} i1 @_Z5firsti(i32 {{.*}} %[[VALUE]])
// CHECK: br i1 %[[FIRST]], label %[[DONE:.*]], label %[[SECOND_BLOCK:.*]]
// CHECK: [[SECOND_BLOCK]]:
// CHECK: %[[SECOND:.*]] = tail call{{.*}} i1 @_Z6secondi(i32 {{.*}} %[[VALUE]])
// CHECK-NEXT: br label %[[DONE]]
// CHECK: [[DONE]]:
// CHECK: phi i1 [ true, %{{.*}} ], [ %[[SECOND]], %[[SECOND_BLOCK]] ]
bool short_circuit(int value) {
  return match(value, case first_pattern or second_pattern);
}

// Expanding an or-pattern into rows must not cause the complete source pattern
// to be evaluated again when its first row fails.
// CHECK-LABEL: define{{.*}} i32 @_Z23selection_short_circuiti(
// CHECK-COUNT-1: call{{.*}} i1 @_Z5firsti
// CHECK-COUNT-1: call{{.*}} i1 @_Z6secondi
int selection_short_circuit(int value) {
  return match (value) {
    case first_pattern or second_pattern => 1;
    case _ => 0;
  };
}

struct Pair {
  int first;
  int second;
};

// CHECK-LABEL: define{{.*}} i32 @_Z16bind_from_either4Pair(
int bind_from_either(Pair pair) {
  return match (pair) {
    case [0, int value] or [int value, 0] => value;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z22case_condition_binding4Pair(
int case_condition_binding(Pair pair) {
  if (case [0, int value] or [int value, 0] = pair)
    return value;
  return -1;
}

struct Triple {
  int first;
  int second;
  int third;
};

// CHECK-LABEL: define{{.*}} i32 @_Z25bind_pack_from_either_end6Triple(
int bind_pack_from_either_end(Triple triple) {
  return match (triple) {
    case [0, auto&& ...values] or [auto&& ...values, 0] =>
        (... + values);
    case _ => -1;
  };
}

namespace std {
template <class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct A {};
struct B {};
struct C {};
struct D {};
struct E {};
struct F {};

enum class ChoiceState : unsigned { a, b, c, d, e, f };

struct Choice {
  unsigned active;
  A a;
  B b;
  C c;
  D d;
  E e;
  F f;
};

template <>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {
      ^^A, ^^B, ^^C, ^^D, ^^E, ^^F};
  static constexpr bool has_residual_states = false;

  static constexpr ChoiceState index(const Choice& choice) noexcept {
    return static_cast<ChoiceState>(choice.active);
  }

  template <ChoiceState I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == ChoiceState::a)
      return (static_cast<Self&&>(choice).a);
    else if constexpr (I == ChoiceState::b)
      return (static_cast<Self&&>(choice).b);
    else if constexpr (I == ChoiceState::c)
      return (static_cast<Self&&>(choice).c);
    else if constexpr (I == ChoiceState::d)
      return (static_cast<Self&&>(choice).d);
    else if constexpr (I == ChoiceState::e)
      return (static_cast<Self&&>(choice).e);
    else
      return (static_cast<Self&&>(choice).f);
  }
};

// CHECK-LABEL: define{{.*}} void @_Z20ignored_alternativesRK6Choice
void ignored_alternatives(const Choice& choice) {
  match (choice) {
    case { const B & } => ;
    case { const C & } => ;
    case { const E & } => ;
    case { const F & } => ;
    case { const A & } or
         { const D & } => ;
  }
}

// Or-pattern operands participate in the same discriminator switch as other
// source cases.
// OR-TREE-LABEL: define{{.*}} i32 @_Z20grouped_alternativesRK6Choice
// OR-TREE: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// OR-TREE-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// OR-TREE: switch i32 %{{.*}}, label %{{.*}} [
// OR-TREE-DAG: i32 0, label %{{.*}}
// OR-TREE-DAG: i32 1, label %{{.*}}
// OR-TREE-DAG: i32 2, label %{{.*}}
// OR-TREE-DAG: i32 3, label %{{.*}}
// OR-TREE-DAG: i32 4, label %{{.*}}
// OR-TREE-DAG: i32 5, label %{{.*}}
int grouped_alternatives(const Choice& choice) {
  return match (choice) {
    case { .a: _ } or { .d: _ } => 5;
    case { const B & } => 1;
    case { const C & } => 2;
    case { const E & } => 3;
    case { const F & } => 4;
  };
}

// Declaration-free left conjuncts can be normalized before a final binding.
// OR-TREE-LABEL: define{{.*}} i32 @_Z19grouped_and_bindingRK6Choice
// OR-TREE-COUNT-1: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// OR-TREE: switch i32 %{{.*}}, label %{{.*}} [
// OR-TREE-DAG: i32 0, label %{{.*}}
// OR-TREE-DAG: i32 3, label %{{.*}}
int grouped_and_binding(const Choice& choice) {
  return match (choice) {
    case ({ .a: _ } or { .d: _ }) and const Choice& whole =>
      static_cast<int>(whole.active);
    case _ => -1;
  };
}

struct ChoicePair {
  Choice first;
  Choice second;
};

// Or-patterns nested under decomposition are expanded before the branching
// column is selected.
// OR-TREE-LABEL: define{{.*}} i32 @_Z19nested_alternativesRK10ChoicePair
// OR-TREE-COUNT-2: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// OR-TREE-COUNT-2: switch i32
int nested_alternatives(const ChoicePair& choices) {
  return match (choices) {
    case [{ .a: _ } or { .d: _ }, { .b: _ }] => 1;
    case _ => 0;
  };
}
