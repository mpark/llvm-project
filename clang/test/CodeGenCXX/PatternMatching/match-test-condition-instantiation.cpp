// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -Werror=unused-value -emit-llvm -O0 %s -o - \
// RUN:   | FileCheck %s

namespace std {
template<class T>
struct alternative_traits;
}

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.active;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).integer);
    else
      return (static_cast<Self&&>(choice).real);
  }
};

int classify(int&);
int classify(double&);

// CHECK-LABEL: define{{.*}} i32 @_Z12condition_ifR6Choice
// CHECK: call{{.*}} i32 @_Z8classifyRi
// CHECK: call{{.*}} i32 @_Z8classifyRd
int condition_if(Choice& choice) {
  if (case { auto&& value } = choice)
    return classify(value);
  return 0;
}

bool standalone_condition(Choice& choice) {
  return choice match case { auto&& value };
}

// CHECK-LABEL: define{{.*}} i32 @_Z22constexpr_condition_ifv
// CHECK-NOT: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5index
// CHECK: ret i32
int constexpr_condition_if() {
  if constexpr (case { int value } = Choice{0, 42, 0.0})
    return value;
  else
    return -1;
}

// CHECK-LABEL: define{{.*}} i32 @_Z28constexpr_condition_if_falsev
// CHECK-NEXT: entry:
// CHECK-NEXT: ret i32 -1
int constexpr_condition_if_false() {
  if constexpr (case { int value } = Choice{1, 42, 1.0})
    return value;
  else
    return -1;
}

// CHECK-LABEL: define{{.*}} i32 @_Z13condition_forR6Choice
// CHECK: call{{.*}} i32 @_Z8classifyRi
// CHECK: add nsw i32
// CHECK: call{{.*}} i32 @_Z8classifyRd
// CHECK: fadd double
int condition_for(Choice& choice) {
  int result = 0;
  for (; case { auto&& value } = choice; ++value) {
    if (value >= 2)
      break;
    result += classify(value);
    if (value == 0)
      continue;
  }
  return result;
}
