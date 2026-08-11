// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

struct Pair {
  int first;
  int second;
};

// CHECK-LABEL: define{{.*}} i32 @_Z8by_valuei(i32 noundef %value)
// CHECK: store i32
// CHECK: load i32
// CHECK: ret i32
int by_value(int value) {
  return value match { case int copy => copy; };
}

// CHECK-LABEL: define{{.*}} i32 @_Z12by_referenceRi(ptr noundef nonnull align 4 dereferenceable(4) %value)
// CHECK: load ptr
// CHECK: load i32
// CHECK: add nsw i32
// CHECK: store i32
int by_reference(int &value) {
  return value match { case int &ref => ++ref; };
}

// CHECK-LABEL: define{{.*}} i32 @_Z9decompose4Pair
// CHECK: add nsw i32
int decompose(Pair pair) {
  return pair match {
    case auto [first, second] => first + second;
  };
}

struct Triple {
  int first;
  int second;
  int third;
};

// CHECK-LABEL: define{{.*}} i32 @_Z16sum_binding_pack6Triple
// CHECK: add nsw i32
// CHECK: add nsw i32
// CHECK: ret i32
int sum_binding_pack(Triple triple) {
  return triple match {
    case auto [...elements] => (... + elements);
  };
}

template<class T>
T dependent(T value) {
  return value match { case auto &&ref => ref; };
}

// CHECK-LABEL: define{{.*}} i32 @_Z21instantiate_dependentv()
// CHECK: call noundef i32 @_Z9dependentIiET_S0_(i32 noundef 5)
int instantiate_dependent() {
  return dependent(5);
}

int evaluations;

int make_subject() {
  return ++evaluations;
}

// CHECK-LABEL: define{{.*}} i32 @_Z21evaluate_subject_oncev()
// CHECK: call noundef i32 @_Z12make_subjectv()
// CHECK-NOT: call noundef i32 @_Z12make_subjectv()
// CHECK: ret i32
int evaluate_subject_once() {
  return make_subject() match {
    case int value if (value < 0) => 0;
    case int value => value;
  };
}

struct MoveOnly {
  int value;
  MoveOnly(int value) : value(value) {}
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) = default;
};

// CHECK-LABEL: define{{.*}} i32 @_Z14forward_rvaluev()
// CHECK: ret i32
int forward_rvalue() {
  return MoveOnly{7} match {
    case MoveOnly &&value => value.value;
  };
}

int copies;
int destructions;

struct CopyCounter {
  int value;
  CopyCounter(const CopyCounter &other) : value(other.value) { ++copies; }
  ~CopyCounter() { ++destructions; }
};

// CHECK-LABEL: define{{.*}} i32 @_Z12guarded_copy11CopyCounter
// CHECK: match.select.init:
// CHECK: call void @_ZN11CopyCounterC1ERKS_
// CHECK: match.guard.check:
// CHECK-NOT: call void @_ZN11CopyCounterC1ERKS_
// CHECK-NOT: call void @_ZN11CopyCounterD1Ev
// CHECK: br i1
// CHECK: match.select.action:
// CHECK-NOT: call void @_ZN11CopyCounterC1ERKS_
// CHECK: br label %match.select.cleanup
// CHECK: match.select.cleanup:
// CHECK: call void @_ZN11CopyCounterD1Ev
int guarded_copy(CopyCounter value) {
  return value match {
    case CopyCounter copy if (copy.value > 0) => copy.value;
    case _ => 0;
  };
}

struct Shape {
  virtual ~Shape();
};

struct Circle : Shape {
  int radius;
};

// CHECK-LABEL: define{{.*}} i32 @_Z20downcast_declarationR5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK-NOT: call ptr @__dynamic_cast
// CHECK: ret i32
int downcast_declaration(Shape &shape) {
  return shape match {
    case Circle &circle if (circle.radius == 0) => 0;
    case Circle &circle => circle.radius;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z28downcast_pointer_declarationP5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK: ret i32
int downcast_pointer_declaration(Shape *shape) {
  return shape match {
    case Circle *circle => circle->radius;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z44downcast_pointer_const_reference_declarationP5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK: ret i32
int downcast_pointer_const_reference_declaration(Shape *shape) {
  return shape match {
    case Circle *const &circle => circle->radius;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z45downcast_pointer_rvalue_reference_declarationP5Shape
// CHECK: call ptr @__dynamic_cast
// CHECK: ret i32
int downcast_pointer_rvalue_reference_declaration(Shape *shape) {
  return shape match {
    case Circle *&&circle => circle->radius;
    case _ => -1;
  };
}


namespace std {
template<class T> struct tuple_size;
template<__SIZE_TYPE__ I, class T> struct tuple_element;
} // namespace std

struct RuntimeProjection;
int &project_first(RuntimeProjection &);
int &project_second(RuntimeProjection &);

struct RuntimeProjection {
  template<__SIZE_TYPE__ I>
  int &get() & {
    if constexpr (I == 0)
      return project_first(*this);
    else
      return project_second(*this);
  }
};

namespace std {
template<> struct tuple_size<RuntimeProjection> {
  static constexpr __SIZE_TYPE__ value = 2;
};
template<__SIZE_TYPE__ I> struct tuple_element<I, RuntimeProjection> {
  using type = int;
};
} // namespace std

// CHECK-LABEL: define{{.*}} i32 @_Z27reuse_structural_projectionR17RuntimeProjection
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm0EEERiv
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm1EEERiv
// CHECK-NOT: call{{.*}} @_ZNR17RuntimeProjection3get
// CHECK: ret i32
int reuse_structural_projection(RuntimeProjection &subject) {
  return subject match {
    case [0, 0] => 0;
    case [auto &&x, 0] => x;
    case [0, auto &&y] => y;
    case [auto &&x, auto &&y] => x + y;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z28reuse_declaration_projectionR17RuntimeProjection
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm0EEERiv
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm1EEERiv
// CHECK-NOT: call{{.*}} @_ZNR17RuntimeProjection3get
// CHECK: ret i32
int reuse_declaration_projection(RuntimeProjection &subject) {
  return subject match {
    case auto &&[x, y] if (x == 0) => 0;
    case auto &&[x, y] => x + y;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z29reuse_binding_pack_projectionR17RuntimeProjection
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm0EEERiv
// CHECK: call{{.*}} @_ZNR17RuntimeProjection3getILm1EEERiv
// CHECK-NOT: call{{.*}} @_ZNR17RuntimeProjection3get
// CHECK: ret i32
int reuse_binding_pack_projection(RuntimeProjection &subject) {
  return subject match {
    case auto &&[...elements] if ((... + elements) == 0) => 0;
    case auto &&[...elements] => (... + elements);
  };
}
