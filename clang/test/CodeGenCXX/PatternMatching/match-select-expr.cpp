// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
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
// CHECK:   %[[SELECT_RES:.*]] = alloca i32, align 4
// CHECK:   %[[LET_X_ADDR:.*]] = alloca ptr, align 8
// CHECK:   store i8 {{.*}}, ptr %[[C_ADDR]], align 1
// CHECK:   %[[C_CHAR:.*]] = load i8, ptr %[[C_ADDR]], align 1
// CHECK:   %[[SEXT_A:.*]] = sext i8 %[[C_CHAR]] to i32
// CHECK:   %[[CMP_MATCH_A:.*]] = icmp eq i32 %[[SEXT_A]], 97
// CHECK:   br i1 %[[CMP_MATCH_A]], label %[[ACTION_A:.*]], label %[[MATCH_B:.*]]

// CHECK: [[ACTION_A]]:
// CHECK:   store i32 1, ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END:.*]]

// CHECK: [[MATCH_B]]:
// CHECK:   %[[SEXT_B:.*]] = load i8, ptr %[[C_ADDR]], align 1
// CHECK:   %conv1 = sext i8 %[[SEXT_B]] to i32
// CHECK:   %[[CMP_MATCH_B:.*]] = icmp eq i32 %conv1, 98
// CHECK:   br i1 %[[CMP_MATCH_B]], label %[[ACTION_B:.*]], label %[[MATCH_LET_X:.*]]

// CHECK: [[ACTION_B]]:
// CHECK:   store i32 2, ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END]]

// CHECK: [[MATCH_LET_X]]:
// CHECK:   store ptr %[[C_ADDR]], ptr %[[LET_X_ADDR]], align 8
// CHECK:   br i1 true, label %[[ACTION_LET_X:.*]], label %[[SELECT_END]]

// CHECK: [[ACTION_LET_X]]:
// CHECK:   %[[X_ADDR:.*]] = load ptr, ptr %[[LET_X_ADDR]], align 8
// CHECK:   %[[X:.*]] = load i8, ptr %[[X_ADDR]], align 1
// CHECK:   %[[SEXT_LET_X:.*]] = sext i8 %[[X]] to i32
// CHECK:   store i32 %[[SEXT_LET_X]], ptr %[[SELECT_RES]], align 4
// CHECK:   br label %[[SELECT_END]]

// CHECK: [[SELECT_END]]:
// CHECK:   %5 = load i32, ptr %[[SELECT_RES]], align 4
// CHECK:   ret i32 %5
// CHECK: }

void test_void_returning_match() {
  0 match { _ => []() {}(); };
}

// CHECK-LABEL: _Z25test_void_returning_matchv
// CHECK: match.select.action:
// CHECK-NEXT:   call void @"_ZZ25test_void_returning_matchvENK3$_0clEv"
// CHECK-NEXT:   br label %match.select.end