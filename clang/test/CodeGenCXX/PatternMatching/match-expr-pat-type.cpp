// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

// CHECK-LABEL: define{{.*}} i1 @_Z11matches_inti
// CHECK: load i32
// CHECK: store i32
// CHECK: ret i1 %{{.*}}
bool matches_int(int value) {
  return value match case int;
}

struct Shape {
  virtual ~Shape();
};

struct Circle : Shape {};

// CHECK-LABEL: define{{.*}} i1 @_Z14matches_circleR5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK: icmp ne ptr
bool matches_circle(Shape& shape) {
  return shape match case Circle&;
}

int copies;

struct Copyable {
  Copyable();
  Copyable(const Copyable&) { ++copies; }
};

// CHECK-LABEL: define{{.*}} i1 @_Z17checks_and_copiesR8Copyable
// CHECK: call void @_ZN8CopyableC
// CHECK: ret i1 %{{.*}}
bool checks_and_copies(Copyable& value) {
  return value match case Copyable;
}

// CHECK-LABEL: define{{.*}} i1 @_Z23reference_does_not_copyR8Copyable
// CHECK-NOT: call{{.*}}CopyableC
// CHECK: ret i1 %{{.*}}
bool reference_does_not_copy(Copyable& value) {
  return value match case Copyable&;
}

int void_evaluations;

void make_void() {
  ++void_evaluations;
}

// CHECK-LABEL: define{{.*}} i32 @_Z17matches_void_oncev
// CHECK: call void @_Z9make_voidv
// CHECK-NOT: call void @_Z9make_voidv
// CHECK: ret i32
int matches_void_once() {
  return make_void() match {
    case void => void_evaluations;
  };
}

// CHECK-LABEL: define{{.*}} i1 @_Z15tests_void_oncev
// CHECK: call void @_Z9make_voidv
// CHECK-NOT: call void @_Z9make_voidv
// CHECK: ret i1 true
bool tests_void_once() {
  return make_void() match case const volatile void;
}
