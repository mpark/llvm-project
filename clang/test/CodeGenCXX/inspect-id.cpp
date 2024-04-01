// RUN: %clang_cc1 -triple i386-unknown-unknown -fpattern-matching -emit-llvm %s -o - | FileCheck %s

// CHECK-LABEL: _Z8test_id0i(
int test_id0(int n) {
  int x = n + 1;       // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(x) { // CHECK: br label %pat.id
    // CHECK: pat.id:
    // CHECK: store{{.*}}%inspect.result
    y => y++; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return x;
}

// CHECK-LABEL: _Z8test_id1i(
int test_id1(int n) {
  int x = n + 1;       // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(x) { // CHECK: br label %pat.id
    // CHECK: pat.id:
    // CHECK: br {{.*}}label %patbody, label %inspect.epilogue
    // CHECK: patbody:
    // CHECK: store{{.*}}%inspect.result
    y if (n != 0) => y++; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return x;
}

// CHECK-LABEL: _Z8test_id2i(
void test_id2(int n) {
  int x = n + 1;
  inspect(x) { // CHECK: br label %pat.id
    // CHECK: pat.id:
    // CHECK: br {{.*}} label %patbody, label %pat.wildcard
    // CHECK: patbody:
    y if (n != 0) => { y++; }; // CHECK: br label %inspect.epilogue
    __ =>; // CHECK: pat.wildcard
  };
}