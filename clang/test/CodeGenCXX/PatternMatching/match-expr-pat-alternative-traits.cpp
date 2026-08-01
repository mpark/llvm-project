// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

namespace std {
template<class T>
struct alternative_traits;
}

struct MaybeInt {
  bool engaged;
  int value;
};

template<>
struct std::alternative_traits<MaybeInt> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  using projection_type = int;

  static __SIZE_TYPE__ index(const MaybeInt&) noexcept;

  template<__SIZE_TYPE__ I>
  static int& get(MaybeInt&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z17match_alternativeR8MaybeInt
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0EEERiRS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0EEERiRS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: ret i32
int match_alternative(MaybeInt& value) {
  return value match {
    case { int& number } if (number == 0) => 0;
    case { int& number } => number;
    case {} => -1;
  };
}
