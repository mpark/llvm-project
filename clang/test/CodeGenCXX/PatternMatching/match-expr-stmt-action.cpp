// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown -fpattern-matching -Wno-c++20-extensions -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

int match_stmt_action(int limit) {
  int r = 0;
  for (int i = limit; i >= 0; i--) {
    r += i match {
      case auto&& x if (x < 5) => 1;
      case 5 => continue;
      case 6 => break;
      case 7 => return 99;
      case _ => 0;
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
// CHECK:   %[[SEVEN_CMP:.*]] = icmp eq i32 {{.*}}, 7
// CHECK:   br i1 %[[SEVEN_CMP]], label %[[RETURN:.*]], label

// CHECK: [[RETURN]]:
// CHECK:   store i32 99, ptr %retval, align 4
// CHECK:   br label %return

auto match_only_return_actions(int value) {
  value match {
    case 0 => return 101;
    case 1 => return 202;
    case _ => return -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z25match_only_return_actionsi
// CHECK: match.select.action:
// CHECK:   store i32 101, ptr %retval
// CHECK: match.select.action{{[0-9]+}}:
// CHECK:   store i32 202, ptr %retval
// CHECK: match.select.action{{[0-9]+}}:
// CHECK:   store i32 -1, ptr %retval

template <class T>
auto match_only_return_actions_template(T value) {
  value match {
    case 0 => return 303;
    case _ => return -2;
  };
}

template auto match_only_return_actions_template<int>(int);

// CHECK-LABEL: define{{.*}} @_Z{{.*}}match_only_return_actions_template
