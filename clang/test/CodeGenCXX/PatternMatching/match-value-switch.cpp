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

int frequent_tuple_column(int first, int second) {
  return match (first, second) {
    case [1, 1] => 11;
    case [_, 2] => 2;
    case [_, 3] => 3;
    case [_, 4] => 4;
    case [2, 5] => 25;
    case [3, 6] => 36;
    case _ => 0;
  };
}

// The second tuple component occurs in more columns of the remaining rows, so
// the decision tree tests it before the first component.
// CHECK-LABEL: define{{.*}} i32 @_Z21frequent_tuple_columnii
// CHECK: store ptr %0, ptr %1, align 8
// CHECK-NEXT: [[SECOND_SLOT:%.*]] = getelementptr {{.*}}, ptr %{{.*}}, i32 0, i32 1
// CHECK-NEXT: [[SECOND_PTR:%.*]] = load ptr, ptr [[SECOND_SLOT]]
// CHECK-NEXT: [[SECOND:%match.value]] = load i32, ptr [[SECOND_PTR]]
// CHECK-NEXT: switch i32 [[SECOND]], label %{{.*}} [
// CHECK-DAG: i32 1, label %{{.*}}
// CHECK-DAG: i32 2, label %{{.*}}
// CHECK-DAG: i32 3, label %{{.*}}
// CHECK-DAG: i32 4, label %{{.*}}
// CHECK: ]

int repeated_value_after_guard(int value) {
  return match (value) {
    case 1 if (guard()) => 1;
    case 1 => 2;
    case _ => 3;
  };
}

// Both source cases share one value test. A failed guard continues directly to
// the second case instead of taking the value-test failure edge.
// CHECK-LABEL: define{{.*}} i32 @_Z26repeated_value_after_guardi
// CHECK-COUNT-1: icmp eq i32
// CHECK: call{{.*}} i1 @_Z5guardv
// CHECK: match.decision.next_pattern:
// CHECK-NEXT: store i32 2, ptr %match.select.result

int repeated_converted_value_after_guard(unsigned value) {
  return match (value) {
    case -1 if (guard()) => 1;
    case ~0u => 2;
    case _ => 3;
  };
}

// Constants are grouped after conversion to the comparison type.
// CHECK-LABEL: define{{.*}} i32 @_Z36repeated_converted_value_after_guardj
// CHECK-COUNT-1: icmp eq i32
// CHECK: call{{.*}} i1 @_Z5guardv
// CHECK: match.decision.next_pattern:
// CHECK-NEXT: store i32 2, ptr %match.select.result
