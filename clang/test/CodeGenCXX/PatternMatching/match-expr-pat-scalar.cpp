// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O1 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

void check(bool b);

// CHECK: _Z5basicii(i32 {{.*}} %[[A:.*]], i32 {{.*}} %[[B:.*]])
void basic(int a, int b) {
  // CHECK: call void @_Z5checkb(i1 {{.*}} true)
  check(0 match 0);
  // CHECK: %[[CMP:.*]] = icmp eq i32 %[[A]], %[[B]]
  // CHECK: call void @_Z5checkb(i1 {{.*}} %[[CMP]])
  check(a match b);
}