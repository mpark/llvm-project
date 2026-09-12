// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

// CHECK-LABEL: define{{.*}} i1 @_Z11matches_inti
// CHECK-NOT: load i32
// CHECK: ret i1 true
bool matches_int(int value) {
  return match(value, case int);
}

struct Shape {
  virtual ~Shape();
};

struct Circle : Shape {};

// CHECK-LABEL: define{{.*}} i1 @_Z14matches_circleR5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK: icmp ne ptr
bool matches_circle(Shape& shape) {
  return match(shape, case Circle&);
}

int copies;

struct Copyable {
  Copyable();
  Copyable(const Copyable&) { ++copies; }
  ~Copyable() { ++copies; }
};

// CHECK-LABEL: define{{.*}} i1 @_Z19checks_without_copyR8Copyable
// CHECK-NOT: call{{.*}}Copyable{{[CD]}}
// CHECK: ret i1 true
bool checks_without_copy(Copyable& value) {
  return match(value, case Copyable);
}

struct Pair {
  Copyable first;
  int second;
};

// CHECK-LABEL: define{{.*}} i1 @_Z26nested_checks_without_copyR4Pair
// CHECK-NOT: call{{.*}}Copyable{{[CD]}}
// CHECK: ret i1 %{{.*}}
bool nested_checks_without_copy(Pair& value) {
  return match(value, case [Copyable, int]);
}

// CHECK-LABEL: define{{.*}} i1 @_Z23reference_does_not_copyR8Copyable
// CHECK-NOT: call{{.*}}Copyable{{[CD]}}
// CHECK: ret i1 true
bool reference_does_not_copy(Copyable& value) {
  return match(value, case Copyable&);
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
  return match (make_void()) {
    case void => void_evaluations;
  };
}

// CHECK-LABEL: define{{.*}} i1 @_Z15tests_void_oncev
// CHECK: call void @_Z9make_voidv
// CHECK-NOT: call void @_Z9make_voidv
// CHECK: ret i1 true
bool tests_void_once() {
  return match(make_void(), case const volatile void);
}
