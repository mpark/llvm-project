// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -O0 -emit-llvm %s -o - | FileCheck %s

// Selections, tests, and conditions use the same integral test grouping.
// CHECK-LABEL: define{{.*}} i32 @_Z6selecti(
// CHECK: switch i32 %match.value
// CHECK-DAG: i32 0, label %{{.*}}
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK: ret i32
int select(int value) {
  return match (value) {
    case 0 => 3;
    case 1 => 4;
    case 2 => 5;
    case _ => 6;
  };
}

// CHECK-LABEL: define{{.*}} i1 @_Z4testi(
// CHECK-NOT: alloca i1
// CHECK: switch i32 %match.value
// CHECK-DAG: i32 0, label %{{.*}}
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK: ret i1
bool test(int value) { return match(value, case 0 or 1 or 2); }

// CHECK-LABEL: define{{.*}} i32 @_Z9conditioni(
// CHECK: switch i32 %match.value
// CHECK-DAG: i32 0, label %{{.*}}
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK: ret i32
int condition(int value) {
  if (case 0 or 1 or 2 = value)
    return 3;
  return 4;
}

namespace std {
template <class T> struct alternative_traits;
struct alternative_info {
  decltype(^^int) info;
  bool empty;
  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct MaybeInt {
  bool engaged;
  int value;
};
template <> struct std::alternative_traits<MaybeInt> {
  static constexpr alternative_info alternatives[] = {{{}, true}, ^^int};
  static constexpr bool has_residual_states = false;
  static __SIZE_TYPE__ index(const MaybeInt &) noexcept;
  template <__SIZE_TYPE__ I> requires (I == 1)
  static int &get(MaybeInt &);
};

// The payload test is dominated by the engagement test, and its values fold
// into a switch even in a single-pattern test expression.
// CHECK-LABEL: define{{.*}} i1 @_Z9projectedR8MaybeInt(
// CHECK-NOT: alloca i1
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: switch i64 %match.index
// CHECK: i64 1, label %[[ENGAGED:.*]]
// CHECK: [[ENGAGED]]:
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm1E
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm1E
// CHECK: switch i32 %match.value
// CHECK-DAG: i32 0, label %{{.*}}
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK: ret i1
bool projected(MaybeInt &value) {
  return match(value, case { 0 or 1 or 2 });
}

bool guard(int);

// CHECK-LABEL: define{{.*}} i1 @_Z7guardedR8MaybeInt(
// CHECK-NOT: alloca i1
// CHECK: switch i64 %match.index
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm1E
// CHECK: call{{.*}} @_Z5guardi
// CHECK-NOT: call{{.*}} @_Z5guardi
// CHECK: ret i1
bool guarded(MaybeInt &value) {
  return match(value, case { auto &payload } if (guard(payload)));
}

// CHECK-LABEL: define{{.*}} i1 @_Z11irrefutablei(
// CHECK-NOT: alloca i1
// CHECK: ret i1 true
bool irrefutable(int value) { return match(value, case int payload); }
