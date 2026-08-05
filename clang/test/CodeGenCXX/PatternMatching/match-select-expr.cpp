// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

auto char_pattern(char c) {
  return c match {
    'a' => 1;
    'b' => 2;
    let x => int(x);
  };
}

// CHECK: _Z12char_patternc
// CHECK:   %[[C_ADDR:.*]] = alloca i8, align 1
// CHECK:   %[[SUBJECT_HOLDER:.*]] = alloca ptr, align 8
// CHECK:   %[[SELECT_RES:.*]] = alloca i32, align 4
// CHECK:   %[[LET_X_ADDR:.*]] = alloca ptr, align 8
// CHECK:   store i8 {{.*}}, ptr %[[C_ADDR]], align 1
// CHECK:   store ptr %[[C_ADDR]], ptr %[[SUBJECT_HOLDER]], align 8
// CHECK:   %[[SUBJECT_A:.*]] = load ptr, ptr %[[SUBJECT_HOLDER]], align 8
// CHECK:   %[[C_CHAR:.*]] = load i8, ptr %[[SUBJECT_A]], align 1
// CHECK:   %[[SEXT_A:.*]] = sext i8 %[[C_CHAR]] to i32
// CHECK:   %[[CMP_MATCH_A:.*]] = icmp eq i32 %[[SEXT_A]], 97
// CHECK:   br i1 %[[CMP_MATCH_A]], label %[[ACTION_A:.*]], label %[[MATCH_B:.*]]

// CHECK: [[ACTION_A]]:
// CHECK:   store i32 1, ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END:.*]]

// CHECK: [[MATCH_B]]:
// CHECK:   %[[SUBJECT_B:.*]] = load ptr, ptr %[[SUBJECT_HOLDER]], align 8
// CHECK:   %[[CHAR_B:.*]] = load i8, ptr %[[SUBJECT_B]], align 1
// CHECK:   %conv1 = sext i8 %[[CHAR_B]] to i32
// CHECK:   %[[CMP_MATCH_B:.*]] = icmp eq i32 %conv1, 98
// CHECK:   br i1 %[[CMP_MATCH_B]], label %[[ACTION_B:.*]], label %[[MATCH_LET_X:.*]]

// CHECK: [[ACTION_B]]:
// CHECK:   store i32 2, ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END]]

// CHECK: [[MATCH_LET_X]]:
// CHECK:   %[[SUBJECT_X:.*]] = load ptr, ptr %[[SUBJECT_HOLDER]], align 8
// CHECK:   store ptr %[[SUBJECT_X]], ptr %[[LET_X_ADDR]], align 8
// CHECK:   br i1 true, label %[[ACTION_LET_X:.*]], label %[[NO_MATCH:.*]]

// CHECK: [[ACTION_LET_X]]:
// CHECK:   %[[X_ADDR:.*]] = load ptr, ptr %[[LET_X_ADDR]], align 8
// CHECK:   %[[X:.*]] = load i8, ptr %[[X_ADDR]], align 1
// CHECK:   %[[SEXT_LET_X:.*]] = sext i8 %[[X]] to i32
// CHECK:   store i32 %[[SEXT_LET_X]], ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END:.*]]

// CHECK: [[NO_MATCH]]:
// CHECK:   call void @_ZSt9terminatev
// CHECK:   unreachable

// CHECK: [[SELECT_END]]:
// CHECK:   %[[RESULT:.*]] = load i32, ptr %[[SELECT_RES]], align 4
// CHECK:   ret i32 %[[RESULT]]
// CHECK: }

void test_void_returning_match() {
  0 match { _ => []() {}(); };
}

// CHECK-LABEL: _Z25test_void_returning_matchv
// CHECK: match.select.action:
// CHECK-NEXT:   call void @"_ZZ25test_void_returning_matchvENK3$_0clEv"
// CHECK-NEXT:   br label %match.select.end

struct GuardInit {
  GuardInit();
  ~GuardInit();
  bool accept() const;
};

// CHECK-LABEL: define{{.*}} i32 @_Z20guard_init_statementi
// CHECK: match.select.guard_init:
// CHECK: call void @_ZN9GuardInitC1Ev
// CHECK: call noundef zeroext i1 @_ZNK9GuardInit6acceptEv
// CHECK: match.select.action:
// CHECK: call void @_ZN9GuardInitD1Ev
// CHECK: ret i32
int guard_init_statement(int value) {
  return value match {
    let copy if (GuardInit init; init.accept()) => copy;
    _ => 0;
  };
}

int &select_lvalue_reference(bool first, int &x, int &y) {
  return first match -> int & {
    true => x;
    false => y;
  };
}

// CHECK-LABEL: define{{.*}} ptr @_Z23select_lvalue_referencebRiS_(
// CHECK:         %match.select.refresult = alloca ptr, align 8
// CHECK:         store ptr {{.*}}, ptr %match.select.refresult, align 8
// CHECK:         br label %[[REF_END:match.select.end]]
// CHECK:       [[REF_END]]:
// CHECK:         %[[REF:.*]] = load ptr, ptr %match.select.refresult, align 8
// CHECK:         ret ptr %[[REF]]

int &&select_rvalue_reference(bool first, int &&x, int &&y) {
  return first match -> int && {
    true => static_cast<int &&>(x);
    false => static_cast<int &&>(y);
  };
}

// CHECK-LABEL: define{{.*}} ptr @_Z23select_rvalue_referencebOiS_(
// CHECK:         %match.select.refresult = alloca ptr, align 8
// CHECK:         store ptr {{.*}}, ptr %match.select.refresult, align 8
// CHECK:       match.select.end:
// CHECK:         %[[RREF:.*]] = load ptr, ptr %match.select.refresult, align 8
// CHECK:         ret ptr %[[RREF]]

template <class T>
T &select_reference_template(bool first, T &x, T &y) {
  return first match -> decltype(auto) {
    true => (x);
    false => (y);
  };
}

template int &select_reference_template(bool, int &, int &);

// CHECK-LABEL: define{{.*}} ptr @_Z25select_reference_templateIiERT_bS1_S1_(
// CHECK:         %match.select.refresult = alloca ptr, align 8
// CHECK:         store ptr {{.*}}, ptr %match.select.refresult, align 8
// CHECK:       match.select.end:
// CHECK:         %[[TREF:.*]] = load ptr, ptr %match.select.refresult, align 8
// CHECK:         ret ptr %[[TREF]]

_Complex double select_complex(bool first, _Complex double x,
                               _Complex double y) {
  return first match -> _Complex double {
    true => x;
    false => y;
  };
}

// CHECK-LABEL: define{{.*}} { double, double } @_Z14select_complexbCdS_(
// CHECK:         %match.select.result = alloca { double, double }, align 8
// CHECK:         %[[COMPLEX_REAL_ADDR:.*]] = getelementptr inbounds nuw { double, double }, ptr %match.select.result, i32 0, i32 0
// CHECK:         %[[COMPLEX_IMAG_ADDR:.*]] = getelementptr inbounds nuw { double, double }, ptr %match.select.result, i32 0, i32 1
// CHECK:         store double {{.*}}, ptr %[[COMPLEX_REAL_ADDR]], align 8
// CHECK:         store double {{.*}}, ptr %[[COMPLEX_IMAG_ADDR]], align 8
// CHECK:       match.select.end:
// CHECK:         %[[COMPLEX_REAL:.*]] = load double, ptr {{.*}}, align 8
// CHECK:         %[[COMPLEX_IMAG:.*]] = load double, ptr {{.*}}, align 8
// CHECK:         ret { double, double } {{.*}}
