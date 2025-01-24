// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

auto decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    [let x, 0] => x * 2;
  };
}

// CHECK-LABEL: _Z21decomposition_patternRA2_Ki
// CHECK:         %[[VAL_0:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_1:.*]] = alloca i32, align 4
// CHECK:         %[[VAL_2:.*]] = alloca i1, align 8
// CHECK:         %[[VAL_3:.*]] = alloca ptr, align 8
// CHECK:         store ptr %[[VAL_4:.*]], ptr %[[VAL_0]], align 8
// CHECK:         %[[VAL_5:.*]] = load ptr, ptr %[[VAL_0]], align 8
// CHECK:         store ptr %[[VAL_5]], ptr %[[VAL_3]], align 8
// CHECK:         br i1 true, label %[[VAL_6:.*]], label %[[VAL_7:.*]]
// CHECK:       match.decomp.next_pattern:
// CHECK:         %[[VAL_9:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_10:.*]] = getelementptr inbounds [2 x i32], ptr %[[VAL_9]], i64 0, i64 1
// CHECK:         %[[VAL_11:.*]] = load i32, ptr %[[VAL_10]], align 4
// CHECK:         %[[VAL_12:.*]] = icmp eq i32 %[[VAL_11]], 0
// CHECK:         br i1 %[[VAL_12]], label %[[VAL_13:.*]], label %[[VAL_7]]
// CHECK:       match.decomp.next_pattern1:
// CHECK:         br label %[[VAL_14:.*]]
// CHECK:       match.decomp.fail:
// CHECK:         store i1 false, ptr %[[VAL_2]], align 8
// CHECK:         br label %[[VAL_15:.*]]
// CHECK:       match.decomp.pass:
// CHECK:         store i1 true, ptr %[[VAL_2]], align 8
// CHECK:         br label %[[VAL_15]]
// CHECK:       match.decomp.end:
// CHECK:         %[[VAL_16:.*]] = load i1, ptr %[[VAL_2]], align 8
// CHECK:         br i1 %[[VAL_16]], label %[[VAL_17:.*]], label %[[VAL_18:.*]]
// CHECK:       match.select.action:
// CHECK:         %[[VAL_19:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_20:.*]] = getelementptr inbounds [2 x i32], ptr %[[VAL_19]], i64 0, i64 0
// CHECK:         %[[VAL_21:.*]] = load i32, ptr %[[VAL_20]], align 4
// CHECK:         %[[VAL_22:.*]] = mul nsw i32 %[[VAL_21]], 2
// CHECK:         store i32 %[[VAL_22]], ptr %[[VAL_1]], align 4
// CHECK:         br label %[[VAL_18]]
// CHECK:       match.select.end:
// CHECK:         %[[VAL_23:.*]] = load i32, ptr %[[VAL_1]], align 4
// CHECK:         ret i32 %[[VAL_23]]