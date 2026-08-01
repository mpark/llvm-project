// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -fcxx-exceptions -O0 -emit-llvm %s -o %t.ll
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
// CHECK:   %[[COND:.*]] = icmp ne i8
// CHECK:   br i1 %[[COND]], label %[[PAT_TEST:.*]], label %[[SUBJ_FAIL:.*]]

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
// CHECK:   %[[COND:.*]] = icmp ne i8
// CHECK:   br i1 %[[COND]], label %[[PAT_TEST:.*]], label %[[SUBJ_FAIL:.*]]

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
// CHECK:   %[[TO_BOOL:.*]] = icmp ne ptr {{.*}}, null
// CHECK:   %[[FIRST_COND:.*]] = icmp ne i8
// CHECK:   br i1 %[[FIRST_COND]], label %[[FIRST_PAT_TEST:.*]], label %[[FIRST_SUBJECT_FAIL:.*]]

// CHECK: [[FIRST_PAT_TEST]]:
// CHECK:   %[[SECOND_TO_BOOL:.*]] = icmp ne ptr {{.*}}, null
// CHECK:   %[[SECOND_COND:.*]] = icmp ne i8
// CHECK:   br i1 %[[SECOND_COND]], label %[[SECOND_PAT_TEST:.*]], label %[[SECOND_SUBJECT_FAIL:.*]]

// CHECK: [[SECOND_PAT_TEST]]:
// CHECK:   %[[SUB_PAT_TEST:.*]] = icmp eq i32 {{.*}}, 1
// CHECK:   store i1 %[[SUB_PAT_TEST]], ptr %[[RES_ADDR_SUB]],
// CHECK:   br label %[[SUB_RES_BB:.*]]

// CHECK: [[SECOND_SUBJECT_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR_SUB]],
// CHECK:   br label %[[SUB_RES_BB]]

// CHECK: [[SUB_RES_BB]]:
// CHECK:   %[[SUB_RESULT:.*]] = load i1, ptr %[[RES_ADDR_SUB]],
// CHECK:   store i1 %[[SUB_RESULT]], ptr %[[RES_ADDR]],
// CHECK:   br label %[[FINAL_RES_BB:.*]]

// CHECK: [[FIRST_SUBJECT_FAIL]]:
// CHECK:   store i1 false, ptr %[[RES_ADDR]],
// CHECK:   br label %[[FINAL_RES_BB]]

// CHECK: [[FINAL_RES_BB]]:
// CHECK:   %[[FINAL_RESULT:.*]] = load i1, ptr %[[RES_ADDR]],
// CHECK:   ret i1 %[[FINAL_RESULT]]

struct OptionalProjection;
bool optional_engaged(const OptionalProjection &);
int &optional_value(OptionalProjection &);

struct OptionalProjection {
  explicit operator bool() const { return optional_engaged(*this); }
  int &operator*() { return optional_value(*this); }
};

int reuse_optional_projection(OptionalProjection &subject) {
  return subject match {
    ? 0 => 0;
    ? let x => x;
    _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z25reuse_optional_projectionR18OptionalProjection
// CHECK: call{{.*}} @_ZNK18OptionalProjectioncvbEv
// CHECK-NOT: call{{.*}} @_ZNK18OptionalProjectioncvbEv
// CHECK: call{{.*}} @_ZN18OptionalProjectiondeEv
// CHECK-NOT: call{{.*}} @_ZN18OptionalProjectiondeEv
// CHECK: ret i32
