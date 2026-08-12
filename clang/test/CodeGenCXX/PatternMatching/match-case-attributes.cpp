// RUN: %clang_cc1 -std=c++2d -fpattern-matching -O1 \
// RUN:   -disable-llvm-passes -emit-llvm %s -o - | FileCheck %s

int likely_case(int value) {
  return value match {
    [[likely]] case 0 => 1;
    case _ => 2;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z11likely_casei
// CHECK: call i1 @llvm.expect.i1(i1 {{.*}}, i1 true)

int unlikely_case(int value) {
  return value match {
    [[unlikely]] case 0 => 1;
    case _ => 2;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z13unlikely_casei
// CHECK: call i1 @llvm.expect.i1(i1 {{.*}}, i1 false)

template<class T>
int dependent_case(T value) {
  return value match {
    [[likely]] case int integer => integer;
    case _ => 0;
  };
}

template int dependent_case(int);

// CHECK-LABEL: define{{.*}} i32 @_Z14dependent_caseIiEiT_
// CHECK: call i1 @llvm.expect.i1(i1 {{.*}}, i1 true)
