// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -Wno-c++20-extensions -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

int match_stmt_action(int limit) {
  int r = 0;
  for (int i = limit; i >= 0; i--) {
    r += i match {
      let x if (x < 5) => 1;
      5 => continue;
      6 => break;
      7 => return 99;
    };
  }
  return r;
}

// CHECK-LABEL: _Z17match_stmt_actioni

// CHECK:  %[[FIVE_CMP:.*]] = icmp eq i32 {{.*}}, 5
// CHECK:  br i1 %[[FIVE_CMP]], label %[[CONTINUE:.*]], label %[[NEXT_SIX:.*]]

// CHECK: [[CONTINUE]]:
// CHECK:   br label %for.inc

// CHECK: [[NEXT_SIX]]:
// CHECK:   %[[SIX_CMP:.*]] = icmp eq i32 {{.*}}, 6
// CHECK:   br i1 %[[SIX_CMP]], label %[[BREAK:.*]], label %[[NEXT_SEVEN:.*]]

// CHECK: [[BREAK]]:
// CHECK:   br label %for.end

// CHECK: [[NEXT_SEVEN]]:
// CHECK:   %[[SEVEN_CMP:.*]] = icmp eq i32 %8, 7
// CHECK:   br i1 %[[SEVEN_CMP]], label %[[RETURN:.*]], label

// CHECK: [[RETURN]]:
// CHECK:   store i32 99, ptr %retval, align 4
// CHECK:   br label %return