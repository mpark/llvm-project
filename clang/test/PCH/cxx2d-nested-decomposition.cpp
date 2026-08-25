// RUN: %clang_cc1 -std=c++2d -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++2d -include-pch %t -verify %s

#ifndef HEADER
#define HEADER

struct Pair {
  int first;
  int second;
};

struct Outer {
  Pair pair;
  int last;
};

template<class T>
constexpr int nested(T value) {
  auto [[x, y], z] = value;
  return x + y + z;
}

#else

static_assert(nested(Outer{{1, 2}, 3}) == 6);

#endif

// expected-no-diagnostics
