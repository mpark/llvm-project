// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - | FileCheck %s

struct Empty {};

// CHECK-LABEL: define{{.*}} i32 @_Z25match_empty_decomposition5Empty(
// CHECK: store i32 42
// CHECK: ret i32
int match_empty_decomposition(Empty value) {
  return value match {
    case [] => 42;
  };
}
