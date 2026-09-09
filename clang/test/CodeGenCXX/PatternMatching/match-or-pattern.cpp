// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown \
// RUN:   -fpattern-matching -O1 -emit-llvm %s -o - | FileCheck %s

// CHECK-LABEL: define{{.*}} i32 @_Z8classifyi(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
// CHECK: %[[IN_RANGE:.*]] = icmp ult i32 %[[VALUE]], 2
// CHECK: %[[RESULT:.*]] = zext i1 %[[IN_RANGE]] to i32
// CHECK: ret i32 %[[RESULT]]
int classify(int value) {
  return match (value) {
    case 0 || 1 => 1;
    case _ => 0;
  };
}

extern bool first(int);
extern bool second(int);

// CHECK-LABEL: define{{.*}} i1 @_Z13short_circuiti(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
// CHECK: %[[FIRST:.*]] = tail call{{.*}} i1 @_Z5firsti(i32 {{.*}} %[[VALUE]])
// CHECK: %[[FIRST_MATCH:.*]] = icmp eq i32 %[[VALUE]],
// CHECK: br i1 %[[FIRST_MATCH]], label %[[DONE:.*]], label %[[SECOND_BLOCK:.*]]
// CHECK: [[SECOND_BLOCK]]:
// CHECK: %[[SECOND:.*]] = tail call{{.*}} i1 @_Z6secondi(i32 {{.*}} %[[VALUE]])
// CHECK: %[[SECOND_MATCH:.*]] = icmp eq i32 %[[VALUE]],
// CHECK: br i1 %[[SECOND_MATCH]], label %[[DONE]], label
bool short_circuit(int value) {
  return value match case first(value) || second(value);
}

struct Pair {
  int first;
  int second;
};

// CHECK-LABEL: define{{.*}} i32 @_Z16bind_from_either4Pair(
int bind_from_either(Pair pair) {
  return match (pair) {
    case [0, int value] || [int value, 0] => value;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z22case_condition_binding4Pair(
int case_condition_binding(Pair pair) {
  if (case [0, int value] || [int value, 0] = pair)
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
    case [0, auto&& ...values] || [auto&& ...values, 0] =>
        (... + values);
    case _ => -1;
  };
}
