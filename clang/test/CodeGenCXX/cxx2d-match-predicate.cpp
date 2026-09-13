// RUN: %clang_cc1 -std=c++2d -fpattern-matching -emit-llvm -O0 -o - %s | FileCheck %s

int constructions;

struct Pattern {
  int value;

  constexpr Pattern(int value) : value(value) {
    if (!__builtin_is_constant_evaluated())
      ++constructions;
  }

  constexpr bool operator==(int other) const { return value == other; }
};

// CHECK-LABEL: define{{.*}} zeroext i1 @_Z5matchi(
// CHECK-NOT: @_ZN7PatternC
// CHECK-NOT: @constructions
// CHECK: ret i1
bool match(int value) {
  return match(value, case Pattern{1});
}

struct Predicate {
  int threshold;

  constexpr Predicate(int threshold) : threshold(threshold) {
    if (!__builtin_is_constant_evaluated())
      ++constructions;
  }

  constexpr bool operator()(int value) const { return value >= threshold; }
};

// CHECK-LABEL: define{{.*}} zeroext i1 @_Z15predicate_matchi(
// CHECK-NOT: @_ZN9PredicateC
// CHECK-NOT: @constructions
// CHECK: ret i1
bool predicate_match(int value) {
  return match(value, case Predicate{1});
}
