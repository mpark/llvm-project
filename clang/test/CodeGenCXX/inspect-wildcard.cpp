// RUN: %clang_cc1 -triple i386-unknown-unknown -fpattern-matching -Wno-unused-value -emit-llvm %s -o - | FileCheck %s

// CHECK-LABEL: _Z13test_wildcardi
int test_wildcard(int n) {
  int x = n+1; // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(n) { // CHECK: br label %pat.wildcard
    // CHECK: pat.wildcard
    // CHECK: store{{.*}}%inspect.result
    __ => x++; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return w;
}

// CHECK-LABEL: _Z14test_wildcard2i
void test_wildcard2(int n) {
  // CHECK-NOT: %inspect.result =
  inspect(n) { // CHECK: br label %pat.wildcard
    __ => {
      // CHECK: pat.wildcard
      n++;
      inspect(n) -> void { // CHECK: br label %pat.wildcard1
        // CHECK: inspect.epilogue
        __ => (void)n++; // CHECK: br label %inspect.epilogue3
      };
    }
  };
  // CHECK-NOT: load{{.*}}%inspect.result
  // CHECK: inspect.epilogue3
}

// CHECK-LABEL: _Z14test_wildcard3i
int test_wildcard3(int n) {
  int x = n + 1;       // CHECK: %inspect.result{{.*}}alloca
  int w = inspect(n) { // CHECK: br label %pat.wildcard
    // CHECK: pat.wildcard
    // CHECK: br {{.*}}label %patbody, label %inspect.epilogue
    // CHECK: patbody:
    // CHECK: store{{.*}}%inspect.result
    __ if (x > 0) => x++; // CHECK: br label %inspect.epilogue
  };
  // CHECK: load{{.*}}%inspect.result
  return w;
}
