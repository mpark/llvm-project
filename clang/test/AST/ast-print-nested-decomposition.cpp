// RUN: %clang_cc1 -std=c++2d -ast-print %s | FileCheck %s
// RUN: %clang_cc1 -std=c++2d -ast-dump %s | FileCheck %s --check-prefix=DUMP

struct Pair {
  int first;
  int second;
};

struct Outer {
  Pair pair;
  int last;
};

auto [[global_x, global_y], global_z] = Outer{{1, 2}, 3};

template<class T>
void nested(T value) {
  auto [[x, y], z] = value;
  auto [[first, ...middle, last], tail] = value;
}

// CHECK: auto {{\[\[}}global_x, global_y], global_z] = Outer
// CHECK: template <class T> void nested(T value) {
// CHECK-NEXT:     auto {{\[\[}}x, y], z] = value;
// CHECK-NEXT:     auto {{\[\[}}first, ...middle, last], tail] = value;
// CHECK-NEXT: }

// DUMP-NOT: __nested_structured_binding
