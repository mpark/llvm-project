// RUN: %clang_cc1 -triple i386-unknown-unknown -fpattern-matching -emit-llvm %s -o - | FileCheck %s

// CHECK-LABEL: _Z9test_exp0i(
int test_exp0(int n) {
  int x = n + 1;       // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(x) { // CHECK: br label %pat.exp
    // CHECK: pat.exp:
    // CHECK: store{{.*}}%inspect.result
    1 => 8; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return x;
}

// CHECK-LABEL: _Z9test_exp1i(
int test_exp1(int n) {
  int x = n + 1;       // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(x) { // CHECK: br label %pat.exp
    // CHECK: pat.exp:
    // CHECK: br {{.*}}label %patbody, label %inspect.epilogue
    // CHECK: patbody:
    // CHECK: store{{.*}}%inspect.result
    2 if (n != 0) => 7; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return x;
}
