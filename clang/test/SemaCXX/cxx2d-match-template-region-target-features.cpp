// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only \
// RUN:   -triple aarch64 -verify=sve %s
// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only \
// RUN:   -triple riscv64 -verify=rvv %s

#if defined(__aarch64__)
using Vector = __SVInt8_t;
#define VECTOR_FEATURE "sve"
#else
using Vector = __rvv_int8m1_t;
#define VECTOR_FEATURE "arch=+zve32x"
#endif

struct Pair {
  int first;
  int second;
};

void without_feature() {
  match (Pair{}) {
    case auto [...elements] => {
      if constexpr (false) {
        // Non-dependent errors are checked when the region is parsed.
        // sve-error@+2 {{cannot be used in a target without sve}}
        // rvv-error@+1 {{requires the 'zve32x' extension}}
        Vector vector;
      }
      (void)sizeof...(elements);
    }
  }
}

void expansion_without_feature() {
  template for (auto element : {1}) {
    if constexpr (false) {
      // sve-error@+2 {{cannot be used in a target without sve}}
      // rvv-error@+1 {{requires the 'zve32x' extension}}
      Vector vector;
    }
    (void)element;
  }
}

__attribute__((target(VECTOR_FEATURE)))
void with_feature() {
  match (Pair{}) {
    case auto [...elements] => {
      if constexpr (false) {
        Vector vector;
      }
      (void)sizeof...(elements);
    }
  }
  template for (auto element : {1}) {
    if constexpr (false) {
      Vector vector;
    }
    (void)element;
  }
}
