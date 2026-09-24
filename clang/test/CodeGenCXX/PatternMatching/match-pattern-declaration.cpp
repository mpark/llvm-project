// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - | FileCheck %s

namespace std {
template <class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template <>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^double};
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.active;
  }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).integer);
    else
      return (static_cast<Self&&>(choice).real);
  }
};

int classify(int&);
int classify(double&);

// CHECK-LABEL: define{{.*}} i32 @_Z13heterogeneousR6Choice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5index
// CHECK: call{{.*}} i32 @_Z8classifyRi
// CHECK: call{{.*}} i32 @_Z8classifyRd
int heterogeneous(Choice& choice) {
  case { auto&& value } = choice;
  return classify(value);
}

struct ResidualChoice : Choice {};

template <>
struct std::alternative_traits<ResidualChoice>
    : std::alternative_traits<Choice> {
  static constexpr bool has_residual_states = true;
};

// CHECK-LABEL: define{{.*}} i32 @_Z21residual_without_elseR14ResidualChoice
// CHECK: call{{.*}} @_ZNSt18alternative_traitsI6ChoiceE5index
// CHECK: call{{.*}} i32 @_Z8classifyRi
// CHECK: call{{.*}} i32 @_Z8classifyRd
// CHECK: call void @llvm.trap()
int residual_without_else(ResidualChoice& choice) {
  case { auto&& value } = choice;
  return classify(value);
}

struct Pair {
  int first;
  int second;
};

// CHECK-LABEL: define{{.*}} i32 @_Z9refutable4Pair
// CHECK: icmp eq i32 {{.*}}, 0
// CHECK: store i32 -1
int refutable(Pair pair) {
  case [0, int value] = pair else return -1;
  return value;
}
