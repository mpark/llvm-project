// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-unknown -fpattern-matching -Wno-c++20-extensions -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

int match_stmt_action(int limit) {
  int r = 0;
  for (int i = limit; i >= 0; i--) {
    r += match (i) {
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

// CHECK: [[MATCH_VALUE:%match.value]] = load i32
// CHECK-NEXT: switch i32 [[MATCH_VALUE]], label %[[DEFAULT:.*]] [
// CHECK-NEXT:   i32 5, label %[[CONTINUE:.*]]
// CHECK-NEXT:   i32 6, label %[[BREAK:.*]]
// CHECK-NEXT:   i32 7, label %[[RETURN:.*]]
// CHECK-NEXT: ]

// CHECK: [[CONTINUE]]:
// CHECK:   br label %for.inc

// CHECK: [[BREAK]]:
// CHECK:   br label %for.end

// CHECK: [[RETURN]]:
// CHECK:   store i32 99, ptr %retval, align 4
// CHECK:   br label %return

// CHECK: [[DEFAULT]]:

auto match_only_return_actions(int value) {
  match (value) {
    case 0 => return 101;
    case 1 => return 202;
    case _ => return -1;
  }
}

// CHECK-LABEL: define{{.*}} i32 @_Z25match_only_return_actionsi
// CHECK:   store i32 101, ptr %retval
// CHECK:   store i32 202, ptr %retval
// CHECK:   store i32 -1, ptr %retval

template <class T>
auto match_only_return_actions_template(T value) {
  match (value) {
    case 0 => return 303;
    case _ => return -2;
  }
}

template auto match_only_return_actions_template<int>(int);

// CHECK-LABEL: define{{.*}} @_Z{{.*}}match_only_return_actions_template

void record(int);

void match_general_statements(int value) {
  match (value) {
    case 0 => {
      record(10);
      record(11);
    }
    case 1 => if (value) record(20); else record(21);
    case _ => record(30);
  }
}

// CHECK-LABEL: define{{.*}} void @_Z24match_general_statementsi
// CHECK: call void @_Z6recordi(i32 noundef 10)
// CHECK: call void @_Z6recordi(i32 noundef 11)
// CHECK: call void @_Z6recordi(i32 noundef 20)
// CHECK: call void @_Z6recordi(i32 noundef 21)
// CHECK: call void @_Z6recordi(i32 noundef 30)

struct Cleanup {
  Cleanup();
  ~Cleanup();
};

void match_declaration_statement(int value) {
  match (value) {
    case 0 => Cleanup cleanup;
    case _ => record(40);
  }
  record(41);
}

// CHECK-LABEL: define{{.*}} void @_Z27match_declaration_statementi
// CHECK: call void @_ZN7CleanupC1Ev
// CHECK: call void @_ZN7CleanupD1Ev
// CHECK: br label %match.select.end
// CHECK: call void @_Z6recordi(i32 noundef 40)
// CHECK: match.select.end:
// CHECK: call void @_Z6recordi(i32 noundef 41)

double potentially_returns();

int match_nonreturning_handler(int value) {
  return match (value) -> int {
    case 0 => not return potentially_returns();
    case _ => 42;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z26match_nonreturning_handleri
// CHECK: call noundef double @_Z19potentially_returnsv()
// CHECK-NEXT: unreachable
// CHECK: store i32 42, ptr %match.select.result
