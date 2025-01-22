// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

void check(bool b);

// CHECK-LABEL: _Z5basicv
void basic() {
  check([]() { return 0 match let _; }());
  // CHECK: "_ZZ5basicvENK3$_0clEv"

  // CHECK: alloca ptr, align 8
  // CHECK: %[[LET_VAR:.*]] = alloca ptr, align 8
  // CHECK: %[[ZERO_VAR:.*]] = alloca i32, align 4
  // CHECK: store ptr
  // CHECK: store i32 0, ptr %[[ZERO_VAR]], align 4
  // CHECK: store ptr %[[ZERO_VAR]], ptr %[[LET_VAR]], align 8
  // CHECK: ret i1 true

  check([]() { return 0 match let x; }());
  // CHECK: "_ZZ5basicvENK3$_1clEv"

  // CHECK: alloca ptr, align 8
  // CHECK: %[[LET_VAR:.*]] = alloca ptr, align 8
  // CHECK: %[[ZERO_VAR:.*]] = alloca i32, align 4
  // CHECK: store ptr
  // CHECK: store i32 0, ptr %[[ZERO_VAR]], align 4
  // CHECK: store ptr %[[ZERO_VAR]], ptr %[[LET_VAR]], align 8
  // CHECK: ret i1 true

  check([]() { int x = 0; return &x match ? let x; }());
  // CHECK: "_ZZ5basicvENK3$_2clEv"

  // CHECK: alloca ptr, align 8
  // CHECK: alloca ptr, align 8
  // CHECK: alloca ptr, align 8
  // CHECK: %[[LET_VAR:.*]] = alloca ptr, align 8

  // CHECK: match.pat.test
  // CHECK: %[[X_ADDR_STORAGE:.*]] = load ptr, ptr %0, align 8
  // CHECK: %[[X_ADDR:.*]] = load ptr, ptr %[[X_ADDR_STORAGE]], align 8
  // CHECK: store ptr %[[X_ADDR]], ptr %[[LET_VAR]], align 8
  // CHECK: store i1 true
  // CHECK: br label
}

