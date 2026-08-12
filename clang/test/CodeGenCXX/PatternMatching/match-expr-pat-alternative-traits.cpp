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

  static __SIZE_TYPE__ index(const MaybeInt&) noexcept;

  template<__SIZE_TYPE__ I>
    requires(I == 0)
  static int& get(MaybeInt&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z17match_alternativeR8MaybeInt
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0E
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE3getILm0E
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI8MaybeIntE5indexERKS0_
// CHECK: ret i32
int match_alternative(MaybeInt& value) {
  return value match {
    case { int& number } if (number == 0) => 0;
    case { int& number } => number;
    case {} => -1;
  };
}

// A projection with only one projectable state uses the same semantic
// instantiation model, including when its binding is injected into a loop body.
// CHECK-LABEL: define{{.*}} i32 @_Z23match_alternative_whileR8MaybeInt
int match_alternative_while(MaybeInt& value) {
  int result = -1;
  while (case { int& number } = value) {
    result = number;
    value.engaged = false;
  }
  return result;
}

struct Choice {
  unsigned state;
  int first;
  double second;
};

template<__SIZE_TYPE__ I>
struct ChoiceAlternative;

template<>
struct ChoiceAlternative<0> {
  using type = int;
};

template<>
struct ChoiceAlternative<1> {
  using type = double;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  static __SIZE_TYPE__ index(const Choice&) noexcept;

  template<__SIZE_TYPE__ I>
  static typename ChoiceAlternative<I>::type& get(Choice&);
};

template<class T>
int match_dependent_alternative(T& value, int& guards) {
  return value match {
    case { auto&& alternative } if (++guards == 2) =>
        static_cast<int>(alternative);
    case _ => 0;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z41instantiate_dependent_alternative_patternR6ChoiceRi
// CHECK: call{{.*}} @_Z27match_dependent_alternativeI6ChoiceEiRT_Ri
int instantiate_dependent_alternative_pattern(Choice& value, int& guards) {
  return match_dependent_alternative(value, guards);
}

template<class Outer>
int match_nested_dependent_alternative(Choice& value, int& guards) {
  return [&]<class Inner>(Inner) {
    return value match {
      case { auto&& alternative }
          if (++guards == 2 && sizeof(Outer) == sizeof(Inner)) =>
              static_cast<int>(alternative);
      case _ => 0;
    };
  }(Outer{});
}

// CHECK-LABEL: define{{.*}} i32 @_Z48instantiate_nested_dependent_alternative_patternR6ChoiceRi
// CHECK: call{{.*}} @_Z34match_nested_dependent_alternativeIiEiR6ChoiceRi
int instantiate_nested_dependent_alternative_pattern(Choice& value,
                                                       int& guards) {
  return match_nested_dependent_alternative<int>(value, guards);
}

struct OpenChoice {};

template<>
struct std::alternative_traits<OpenChoice> {
  static bool has_value(const OpenChoice&);

  template<class T, class Self>
  static T* try_cast(Self&&);
};

// CHECK-LABEL: define{{.*}} i32 @_Z22match_open_alternativeR10OpenChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_cast
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE8try_cast
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE9has_valueERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI10OpenChoiceE9has_valueERKS0_
// CHECK: ret i32
int match_open_alternative(OpenChoice& value) {
  return value match {
    case { int& number } if (number == 0) => 0;
    case { int& number } => number;
    case { _ } => -2;
    case {} => -1;
  };
}

struct ChoiceProduct {
  Choice first;
  Choice second;
};

int combine(int&, int&);
int combine(int&, double&);
int combine(double&, int&);
int combine(double&, double&);

// Both sibling discriminators are initialized before the first selected-index
// comparison, and neither is recomputed in another Cartesian branch.
// CHECK-LABEL: define{{.*}} i32 @_Z20match_choice_productR13ChoiceProduct
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: icmp eq
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5indexERKS0_
// CHECK: ret i32
int match_choice_product(ChoiceProduct& value) {
  return value match {
    case [{ auto&& first }, { auto&& second }] => combine(first, second);
  };
}
