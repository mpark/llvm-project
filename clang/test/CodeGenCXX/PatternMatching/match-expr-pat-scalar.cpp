// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown -fpattern-matching -O1 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

void check(bool b);

// CHECK: _Z5basicii(i32 {{.*}} %[[A:.*]], i32 {{.*}} %[[B:.*]])
void basic(int a, int b) {
  // CHECK: call void @_Z5checkb(i1 {{.*}} true)
  check(0 match case 0);
  // CHECK: %[[CMP:.*]] = icmp eq i32 %[[A]], %[[B]]
  // CHECK: call void @_Z5checkb(i1 {{.*}} %[[CMP]])
  check(a match case b);
}

// CHECK-LABEL: define{{.*}} i32 @_Z14case_conditioni(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
int case_condition(int value) {
  if (case int copy = value if (copy > 0))
    return copy;
  return -1;
  // CHECK: %[[NOT_POSITIVE:.*]] = icmp slt i32 %[[VALUE]], 1
  // CHECK: %[[RESULT:.*]] = select i1 %[[NOT_POSITIVE]], i32 -1, i32 %[[VALUE]]
  // CHECK: ret i32 %[[RESULT]]
}

// CHECK-LABEL: define{{.*}} zeroext i1 @_Z9zero_casei(
// CHECK-SAME: i32 {{.*}} %[[VALUE:.*]])
bool zero_case(int value) {
  if (case 0 = value)
    return true;
  return false;
  // CHECK: %[[RESULT:.*]] = icmp eq i32 %[[VALUE]], 0
  // CHECK: ret i1 %[[RESULT]]
}
