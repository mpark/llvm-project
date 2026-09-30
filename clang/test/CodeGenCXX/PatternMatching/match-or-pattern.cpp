// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown \
// RUN:   -fpattern-matching -O1 -emit-llvm %s -o - | FileCheck %s

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
// CHECK: br i1 %[[SECOND]], label %[[DONE]], label
bool short_circuit(int value) {
  return match(value, case first_pattern or second_pattern);
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

  static constexpr unsigned index(const Choice& choice) noexcept {
    return choice.active;
  }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).a);
    else if constexpr (I == 1)
      return (static_cast<Self&&>(choice).b);
    else if constexpr (I == 2)
      return (static_cast<Self&&>(choice).c);
    else if constexpr (I == 3)
      return (static_cast<Self&&>(choice).d);
    else if constexpr (I == 4)
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
