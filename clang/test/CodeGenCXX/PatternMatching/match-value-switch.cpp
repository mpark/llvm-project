// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown \
// RUN:   -fpattern-matching -O0 -emit-llvm %s -o - | FileCheck %s

bool guard();

int literal_switch(int value) {
  return match (value) {
    case 1 => 10;
    case 2 => 20;
    case 3 if (guard()) => 30;
    case _ => 40;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z14literal_switchi
// CHECK: [[VALUE:%match.value]] = load i32
// CHECK-NEXT: switch i32 [[VALUE]], label %[[DEFAULT:.*]] [
// CHECK-DAG: i32 1, label %[[ONE:.*]]
// CHECK-DAG: i32 2, label %[[TWO:.*]]
// CHECK-DAG: i32 3, label %[[THREE:.*]]
// CHECK: ]
// CHECK: [[THREE]]:
// CHECK: call{{.*}} i1 @_Z5guardv
// CHECK: [[DEFAULT]]:

struct PatternValue {
  int value;
};

constexpr PatternValue one{1};
constexpr PatternValue two{2};
bool operator==(int, PatternValue);

int overloaded_equality(int value) {
  return match (value) {
    case one => 1;
    case two => 2;
    case _ => 3;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z19overloaded_equalityi
// CHECK-NOT: switch
// CHECK-COUNT-2: call{{.*}} i1 @_Zeqi12PatternValue

int volatile_subject(volatile int &value) {
  return match (value) {
    case 1 => 1;
    case 2 => 2;
    case _ => 3;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z16volatile_subjectRVi
// CHECK-NOT: switch
// CHECK-COUNT-2: load volatile i32

constexpr int constant_pattern() {
  return __builtin_is_constant_evaluated() ? 1 : 2;
}

int constant_expression_patterns(int value) {
  return match (value) {
    case constant_pattern() => 1;
    case 2 => 2;
    case _ => 3;
  };
}

// Constant pattern expressions use their constant-evaluated values.
// CHECK-LABEL: define{{.*}} i32 @_Z28constant_expression_patternsi
// CHECK: switch i32 %match.value, label %{{.*}} [
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK: ]
