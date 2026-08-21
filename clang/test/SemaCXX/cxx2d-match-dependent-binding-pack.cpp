// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only -verify %s
// expected-no-diagnostics

namespace std {
template<class> struct tuple_size;
}

struct Triple {
  int first;
  int second;
  int third;
};

constexpr int placeholder_pack(auto value) {
  return value match {
    case auto&& [first, ..._, last] => first + last;
  };
}

constexpr int unnamed_pack(auto value) {
  return value match {
    case auto&& [first, ..., last] => first + last;
  };
}

static_assert(placeholder_pack(Triple{1, 2, 3}) == 4);
static_assert(unnamed_pack(Triple{1, 2, 3}) == 4);
