// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -O0 -o - %s | FileCheck %s --check-prefix=BATCH \
// RUN:   --implicit-check-not='call ptr @__dynamic_cast_first('
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 -o - %s \
// RUN:   | FileCheck %s --check-prefix=LINEAR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -O2 -o - %s | FileCheck %s --check-prefix=OPT
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -O0 -o - %s | FileCheck %s --check-prefix=MS
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -O0 -o - %s | FileCheck %s --check-prefix=I386
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -fsanitize=null -O0 -o - %s | FileCheck %s --check-prefix=SAN
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++2d \
// RUN:   -fpattern-matching -fexperimental-batched-dynamic-cast -emit-llvm \
// RUN:   -debug-info-kind=standalone -O0 -o /dev/null %s

struct Shape {
  virtual ~Shape();
};
struct Circle : Shape { int radius; };
struct Triangle : Shape { int height; };
struct Rectangle : Shape { int width; };
struct Square : Rectangle {};

// BATCH: @[[TARGETS:__dynamic_cast.targets[^ ]*]] = private unnamed_addr constant [3 x { ptr, i64 }] [{ ptr, i64 } { ptr @_ZTI6Circle, i64 0 }, { ptr, i64 } { ptr @_ZTI8Triangle, i64 0 }, { ptr, i64 } { ptr @_ZTI9Rectangle, i64 0 }], align 8
// BATCH: @[[OFFSET_TARGETS:__dynamic_cast.targets[^ ]*]] = private unnamed_addr constant [2 x { ptr, i64 }] [{ ptr, i64 } { ptr @_ZTI12OffsetCircle, i64 8 }, { ptr, i64 } { ptr @_ZTI14OffsetTriangle, i64 8 }], align 8
// MS-NOT: __dynamic_cast.targets
// I386: @[[TARGETS:__dynamic_cast.targets[^ ]*]] = private unnamed_addr constant [3 x { ptr, i32 }] [{ ptr, i32 } { ptr @_ZTI6Circle, i32 0 }, { ptr, i32 } { ptr @_ZTI8Triangle, i32 0 }, { ptr, i32 } { ptr @_ZTI9Rectangle, i32 0 }], align 4

// BATCH-LABEL: define{{.*}} @_Z8dispatchRK5Shape(
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: [[ADJUSTED:%.*]] = call ptr @__dynamic_cast_first(ptr {{.*}}, ptr @_ZTI5Shape, ptr @[[TARGETS]], i64 3, ptr [[SELECTED:%.*]])
// BATCH: [[INDEX:%.*]] = load i64, ptr [[SELECTED]]
// BATCH: switch i64 [[INDEX]], label %{{.*}} [
// BATCH-NEXT: i64 0, label %{{.*}}
// BATCH-NEXT: i64 1, label %{{.*}}
// BATCH-NEXT: i64 2, label %{{.*}}
// BATCH: store ptr [[ADJUSTED]], ptr %c, align 8
// BATCH: store ptr [[ADJUSTED]], ptr %t, align 8
// BATCH: store ptr [[ADJUSTED]], ptr %r, align 8
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH-NOT: call ptr @__dynamic_cast_first(
// BATCH: ret i32
// LINEAR-LABEL: define{{.*}} @_Z8dispatchRK5Shape(
// LINEAR-NOT: __dynamic_cast_first
// LINEAR: call ptr @__dynamic_cast(
// LINEAR: call ptr @__dynamic_cast(
// LINEAR: call ptr @__dynamic_cast(
// LINEAR: ret i32
// OPT-LABEL: define{{.*}} @_Z8dispatchRK5Shape(
// OPT: call ptr @__dynamic_cast_first(
// OPT-NOT: call ptr @__dynamic_cast(
// OPT-NOT: call ptr @__dynamic_cast_first(
// OPT: ret i32
// MS-LABEL: define{{.*}} @"?dispatch@@
// MS-NOT: __dynamic_cast_first
// MS: call ptr @__RTDynamicCast(
// MS: call ptr @__RTDynamicCast(
// MS: call ptr @__RTDynamicCast(
// MS: ret i32
// I386-LABEL: define{{.*}} @_Z8dispatchRK5Shape(
// I386: call ptr @__dynamic_cast_first(ptr {{.*}}, ptr @_ZTI5Shape, ptr @[[TARGETS]], i32 3, ptr
// I386: switch i32
// I386-NOT: call ptr @__dynamic_cast(
// I386: ret i32
int dispatch(const Shape &shape) {
  return match (shape) {
    case const Circle &c => c.radius;
    case const Triangle &t => t.height;
    case const Rectangle &r => r.width;
    case _ => 0;
  };
}

// BATCH-LABEL: define{{.*}} @_Z15square_as_shapev(
// BATCH: call{{.*}} @_Z8dispatchRK5Shape(
// BATCH: ret i32
int square_as_shape() {
  Square square;
  square.width = 47;
  const Shape &shape = square;
  return dispatch(shape);
}

Shape &getShape();

// BATCH-LABEL: define{{.*}} @_Z12subject_oncev(
// BATCH: call{{.*}} @_Z8getShapev(
// BATCH-NOT: call{{.*}} @_Z8getShapev(
// BATCH: call ptr @__dynamic_cast_first(
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: ret i32
int subject_once() {
  return match (getShape()) {
    case Circle & => 1;
    case Triangle & => 2;
    case _ => 0;
  };
}

bool guard();

// BATCH-LABEL: define{{.*}} @_Z7guardedRK5Shape(
// BATCH-NOT: __dynamic_cast_first
// BATCH: call ptr @__dynamic_cast(
// BATCH: call ptr @__dynamic_cast(
// BATCH: call{{.*}} @_Z5guardv(
// BATCH: call ptr @__dynamic_cast(
// BATCH-NOT: __dynamic_cast_first
// BATCH: ret i32
int guarded(const Shape &shape) {
  return match (shape) {
    case const Circle & => 1;
    case const Triangle & if (guard()) => 2;
    case const Rectangle & => 3;
    case _ => 0;
  };
}

// BATCH-LABEL: define{{.*}} @_Z12before_guardRK5Shape(
// BATCH: call ptr @__dynamic_cast_first({{.*}} i64 2,
// BATCH: call ptr @__dynamic_cast(
// BATCH: call{{.*}} @_Z5guardv(
// BATCH: ret i32
int before_guard(const Shape &shape) {
  return match (shape) {
    case const Circle & => 1;
    case const Triangle & => 2;
    case const Rectangle & if (guard()) => 3;
    case _ => 0;
  };
}

struct FinalCircle final : Shape {};
struct FinalTriangle final : Shape {};

// BATCH-LABEL: define{{.*}} @_Z13final_targetsRK5Shape(
// BATCH-NOT: __dynamic_cast_first
// BATCH: ret i32
int final_targets(const Shape &shape) {
  return match (shape) {
    case const FinalCircle & => 1;
    case const FinalTriangle & => 2;
    case _ => 0;
  };
}

struct Left {
  virtual ~Left();
};
struct OffsetCircle : Left, Shape { int value; };
struct OffsetTriangle : Left, Shape { int value; };

// BATCH-LABEL: define{{.*}} @_Z7offsetsRK5Shape(
// BATCH: call ptr @__dynamic_cast_first({{.*}} ptr @[[OFFSET_TARGETS]], i64 2,
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: ret i32
int offsets(const Shape &shape) {
  return match (shape) {
    case const OffsetCircle &c => c.value;
    case const OffsetTriangle &t => t.value;
    case _ => 0;
  };
}

// BATCH-LABEL: define{{.*}} @_Z8ordinaryRK5Shape(
// BATCH-NOT: __dynamic_cast_first
// BATCH: call ptr @__dynamic_cast(
// BATCH: ret ptr
const Circle *ordinary(const Shape &shape) {
  return dynamic_cast<const Circle *>(&shape);
}

// BATCH-LABEL: define{{.*}} @_Z6rvalueO5Shape(
// BATCH: call ptr @__dynamic_cast_first(
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: ret i32
int rvalue(Shape &&shape) {
  return match (static_cast<Shape &&>(shape)) {
    case Circle &&c => c.radius;
    case Triangle &&t => t.height;
    case _ => 0;
  };
}

// BATCH-LABEL: define{{.*}} @_Z9statementR5Shape(
// BATCH: call ptr @__dynamic_cast_first(
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: ret void
void statement(Shape &shape) {
  match (shape) {
    case Circle &c => { c.radius += 1; }
    case Triangle &t => { t.height += 1; }
    case _ => ;
  }
}

// BATCH-LABEL: define{{.*}} @_Z8by_valueRK5Shape(
// BATCH-NOT: __dynamic_cast_first
// BATCH: call ptr @__dynamic_cast(
// BATCH: call ptr @__dynamic_cast(
// BATCH-NOT: __dynamic_cast_first
// BATCH: ret i32
int by_value(const Shape &shape) {
  return match (shape) {
    case Circle c => c.radius;
    case Triangle t => t.height;
    case _ => 0;
  };
}

// BATCH-LABEL: define{{.*}} @_Z10referencesR5Shape(
// BATCH: call ptr @__dynamic_cast_first(
// BATCH-NOT: call ptr @__dynamic_cast(
// BATCH: ret i32
// SAN-LABEL: define{{.*}} @_Z10referencesR5Shape(
// SAN: call ptr @__dynamic_cast_first(
// SAN: call void @__ubsan_handle_type_mismatch_v1_abort(
// SAN: call void @__ubsan_handle_type_mismatch_v1_abort(
// SAN: ret i32
int references(Shape &shape) {
  return match (shape) {
    case Circle &c => 1;
    case Triangle &t => 2;
    case _ => 0;
  };
}
