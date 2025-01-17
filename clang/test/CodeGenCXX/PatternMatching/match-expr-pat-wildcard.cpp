// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O1 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

void check(bool b);

// CHECK-LABEL: _Z5basicii
void basic(int a, int b) {
  // CHECK: call void @_Z5checkb(i1 {{.*}} true)
  check(0 match _);
}