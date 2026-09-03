// RUN: %clang_cc1 -std=c++2c -freflection -triple x86_64-unknown-linux-gnu \
// RUN:   -emit-llvm -o /dev/null %s -verify

// A reflection value has no meaningful runtime representation, but it can be
// a subobject of a constexpr object that reaches CodeGen.
struct WithReflection {
  decltype(^^int) reflection;
  int value;
};

constexpr WithReflection object{^^int, 42};

int read_value() {
  return object.value;
}

// expected-no-diagnostics
