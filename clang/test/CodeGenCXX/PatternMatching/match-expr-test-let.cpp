// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2b -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

int match_in_if_condition(const int *p) {
  if (p match ? let v) {
    return v;
  }
  return -1;
}

// CHECK-LABEL: _Z21match_in_if_conditionPKi
// CHECK:         %[[VAL_0:.*]] = alloca i32, align 4
// CHECK:         %[[VAL_1:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_2:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_3:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_4:.*]] = alloca i1, align 8
// CHECK:         %[[VAL_5:.*]] = alloca ptr, align 8
// CHECK:         store ptr %[[VAL_6:.*]], ptr %[[VAL_1]], align 8
// CHECK:         store ptr %[[VAL_1]], ptr %[[VAL_2]], align 8
// CHECK:         %[[VAL_7:.*]] = load ptr, ptr %[[VAL_2]], align 8
// CHECK:         store ptr %[[VAL_7]], ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_8:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_9:.*]] = load ptr, ptr %[[VAL_8]], align 8
// CHECK:         %[[VAL_10:.*]] = icmp ne ptr %[[VAL_9]], null
// CHECK:         br i1 %[[VAL_10]], label %[[VAL_11:.*]], label %[[VAL_12:.*]]
// CHECK:       match.pat.test:
// CHECK:         %[[VAL_14:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_15:.*]] = load ptr, ptr %[[VAL_14]], align 8
// CHECK:         store ptr %[[VAL_15]], ptr %[[VAL_5]], align 8
// CHECK:         store i1 true, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_16:.*]]
// CHECK:       match.subject.fail:
// CHECK:         store i1 false, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_16]]
// CHECK:       match.result:
// CHECK:         %[[VAL_17:.*]] = load i1, ptr %[[VAL_4]], align 8
// CHECK:         br i1 %[[VAL_17]], label %[[VAL_18:.*]], label %[[VAL_19:.*]]
// CHECK:       if.then:
// CHECK:         %[[VAL_20:.*]] = load ptr, ptr %[[VAL_5]], align 8
// CHECK:         %[[VAL_21:.*]] = load i32, ptr %[[VAL_20]], align 4
// CHECK:         store i32 %[[VAL_21]], ptr %[[VAL_0]], align 4
// CHECK:         br label %[[VAL_22:.*]]
// CHECK:       if.end:
// CHECK:         store i32 -1, ptr %[[VAL_0]], align 4
// CHECK:         br label %[[VAL_22]]
// CHECK:       return:
// CHECK:         %[[VAL_23:.*]] = load i32, ptr %[[VAL_0]], align 4
// CHECK:         ret i32 %[[VAL_23]]