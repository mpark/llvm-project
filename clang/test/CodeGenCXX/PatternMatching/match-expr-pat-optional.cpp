// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

void check(bool b);

// CHECK-LABEL: _Z5basicii
void basic(int a, int b) {
  check([]() { int x = 0; return &x match ? _; }());
  check([]() { int x = 0; return &x match ? 0; }());
  check(![]() { int x = 0, *p = &x; return &p match ?? 1; }());
}

// CHECK-LABEL: "_ZZ5basiciiENK3$_0clEv"

// CHECK:   %[[RES_ADDR:.*]] = alloca i1,
// CHECK:   %[[TOBOOL:.*]] = icmp ne ptr {{.*}}, null
// CHECK:   br i1 %[[TOBOOL]], label %[[PAT_TEST:.*]], label %[[SUBJ_FAIL:.*]]

// CHECK: [[PAT_TEST]]:
// CHECK:   store i1 true, ptr %[[RES_ADDR]]
// CHECK:   br label %[[RES_BB:.*]]

// CHECK: [[SUBJ_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR]]
// CHECK:   br label %[[RES_BB]]

// CHECK: [[RES_BB]]:
// CHECK:   %[[RET:.*]] = load i1, ptr %[[RES_ADDR]]
// CHECK:   ret i1 %[[RET]]

// CHECK: "_ZZ5basiciiENK3$_1clEv"

// CHECK:   %[[RES_ADDR:.*]] = alloca i1,
// CHECK:   %[[TOBOOL:.*]] = icmp ne ptr {{.*}}, null
// CHECK:   br i1 %[[TOBOOL]], label %[[PAT_TEST:.*]], label %[[SUBJ_FAIL:.*]]

// CHECK: [[PAT_TEST]]:
// CHECK:  %[[VAL:.*]] = load i32, ptr {{.*}}, align
// CHECK:  %[[SUB_PAT_TEST:.*]] = icmp eq i32 %[[VAL]], 0
// CHECK:  store i1 %[[SUB_PAT_TEST]], ptr %[[RES_ADDR]]
// CHECK:   br label %[[RES_BB:.*]]

// CHECK: [[SUBJ_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR]]
// CHECK:   br label %[[RES_BB]]

// CHECK: [[RES_BB]]:
// CHECK:   %[[RET:.*]] = load i1, ptr %[[RES_ADDR]]
// CHECK:   ret i1 %[[RET]]

// CHECK: "_ZZ5basiciiENK3$_2clEv"

// CHECK:   %[[RES_ADDR:.*]] = alloca i1
// CHECK:   %[[RES_ADDR_SUB:.*]] = alloca i1
// CHECK:   %[[TO_BOOL:.*]] = icmp ne ptr %3, null
// CHECK:   br i1 %[[TO_BOOL]], label %[[FIRST_PAT_TEST:.*]], label %[[FIRST_SUBJECT_FAIL:.*]]

// CHECK: [[FIRST_PAT_TEST]]:
// CHECK:   %[[SECOND_TO_BOOL:.*]] = icmp ne ptr {{.*}}, null
// CHECK:   br i1 %[[SECOND_TO_BOOL]], label %[[SECOND_PAT_TEST:.*]], label %[[SECOND_SUBJECT_FAIL:.*]]

// CHECK: [[SECOND_PAT_TEST]]:
// CHECK:   %[[SUB_PAT_TEST:.*]] = icmp eq i32 {{.*}}, 1
// CHECK:   store i1 %[[SUB_PAT_TEST]], ptr %[[RES_ADDR_SUB]],
// CHECK:   br label %[[SUB_RES_BB:.*]]

// CHECK: [[SECOND_SUBJECT_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR_SUB]],
// CHECK:   br label %[[SUB_RES_BB]]

// CHECK: [[SUB_RES_BB]]:
// CHECK:   %11 = load i1, ptr %[[RES_ADDR_SUB]],
// CHECK:   store i1 %11, ptr %[[RES_ADDR]],
// CHECK:   br label %[[FINAL_RES_BB:.*]]

// CHECK: [[FIRST_SUBJECT_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR]],
// CHECK:   br label %[[FINAL_RES_BB]]

// CHECK: [[FINAL_RES_BB]]:
// CHECK:   %12 = load i1, ptr %[[RES_ADDR]],
// CHECK:   ret i1 %12
