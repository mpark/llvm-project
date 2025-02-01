// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -Wno-c++20-extensions -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

enum Color { Red, Blue };

struct S {
  Color color;
  int xs[2];
};

struct Result {
  Color color;
  int i;
  bool operator==(const Result&) const noexcept = default;
};

auto nested_decomposition_pattern(const S& s) {
  return s match -> Result {
    [let c, [0, 0]] => {c, -1};
  };
}

// CHECK-LABEL: _Z28nested_decomposition_patternRK1S
// CHECK:         %[[VAL_0:.*]] = alloca %[[VAL_1:.*]], align 4
// CHECK:         %[[VAL_2:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_3:.*]] = alloca %[[VAL_1]], align 4
// CHECK:         %[[VAL_4:.*]] = alloca i1, align 8
// CHECK:         %[[VAL_5:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_6:.*]] = alloca i1, align 8
// CHECK:         %[[VAL_7:.*]] = alloca ptr, align 8
// CHECK:         store ptr %[[VAL_8:.*]], ptr %[[VAL_2]], align 8
// CHECK:         %[[VAL_9:.*]] = load ptr, ptr %[[VAL_2]], align 8
// CHECK:         store ptr %[[VAL_9]], ptr %[[VAL_5]], align 8
// CHECK:         br i1 true, label %[[VAL_10:.*]], label %[[VAL_11:.*]]
// CHECK:       match.decomp.next_pattern:
// CHECK:         %[[VAL_13:.*]] = load ptr, ptr %[[VAL_5]], align 8
// CHECK:         %[[VAL_14:.*]] = getelementptr inbounds nuw %[[VAL_15:.*]], ptr %[[VAL_13]], i32 0, i32 1
// CHECK:         store ptr %[[VAL_14]], ptr %[[VAL_7]], align 8
// CHECK:         %[[VAL_16:.*]] = load ptr, ptr %[[VAL_7]], align 8
// CHECK:         %[[VAL_17:.*]] = getelementptr inbounds [2 x i32], ptr %[[VAL_16]], i64 0, i64 0
// CHECK:         %[[VAL_18:.*]] = load i32, ptr %[[VAL_17]], align 4
// CHECK:         %[[VAL_19:.*]] = icmp eq i32 %[[VAL_18]], 0
// CHECK:         br i1 %[[VAL_19]], label %[[VAL_20:.*]], label %[[VAL_21:.*]]
// CHECK:       match.decomp.next_pattern2:
// CHECK:         %[[VAL_22:.*]] = load ptr, ptr %[[VAL_7]], align 8
// CHECK:         %[[VAL_23:.*]] = getelementptr inbounds [2 x i32], ptr %[[VAL_22]], i64 0, i64 1
// CHECK:         %[[VAL_24:.*]] = load i32, ptr %[[VAL_23]], align 4
// CHECK:         %[[VAL_25:.*]] = icmp eq i32 %[[VAL_24]], 0
// CHECK:         br i1 %[[VAL_25]], label %[[VAL_26:.*]], label %[[VAL_21]]
// CHECK:       match.decomp.next_pattern5:
// CHECK:         br label %[[VAL_27:.*]]
// CHECK:       match.decomp.fail:
// CHECK:         store i1 false, ptr %[[VAL_6]], align 8
// CHECK:         br label %[[VAL_28:.*]]
// CHECK:       match.decomp.pass:
// CHECK:         store i1 true, ptr %[[VAL_6]], align 8
// CHECK:         br label %[[VAL_28]]
// CHECK:       match.decomp.end:
// CHECK:         %[[VAL_29:.*]] = load i1, ptr %[[VAL_6]], align 8
// CHECK:         br i1 %[[VAL_29]], label %[[VAL_30:.*]], label %[[VAL_11]]
// CHECK:       match.decomp.next_pattern6:
// CHECK:         br label %[[VAL_31:.*]]
// CHECK:       match.decomp.fail7:
// CHECK:         store i1 false, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_32:.*]]
// CHECK:       match.decomp.pass8:
// CHECK:         store i1 true, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_32]]
// CHECK:       match.decomp.end9:
// CHECK:         %[[VAL_33:.*]] = load i1, ptr %[[VAL_4]], align 8
// CHECK:         br i1 %[[VAL_33]], label %[[VAL_34:.*]], label %[[VAL_35:.*]]
// CHECK:       match.select.action:
// CHECK:         %[[VAL_36:.*]] = getelementptr inbounds nuw %[[VAL_1]], ptr %[[VAL_3]], i32 0, i32 0
// CHECK:         %[[VAL_37:.*]] = load ptr, ptr %[[VAL_5]], align 8
// CHECK:         %[[VAL_38:.*]] = getelementptr inbounds nuw %[[VAL_15]], ptr %[[VAL_37]], i32 0, i32 0
// CHECK:         %[[VAL_39:.*]] = load i32, ptr %[[VAL_38]], align 4
// CHECK:         store i32 %[[VAL_39]], ptr %[[VAL_36]], align 4
// CHECK:         %[[VAL_40:.*]] = getelementptr inbounds nuw %[[VAL_1]], ptr %[[VAL_3]], i32 0, i32 1
// CHECK:         store i32 -1, ptr %[[VAL_40]], align 4
// CHECK:         br label %[[VAL_35]]
// CHECK:       match.select.end:
// CHECK:         call void @llvm.memcpy.p0.p0.i64(ptr align 4 %[[VAL_0]], ptr align 4 %[[VAL_3]], i64 8, i1 false)
// CHECK:         %[[VAL_41:.*]] = load i64, ptr %[[VAL_0]], align 4
// CHECK:         ret i64 %[[VAL_41]]