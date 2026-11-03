// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -emit-llvm -O0 %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -fexceptions -fcxx-exceptions \
// RUN:   -emit-llvm -O0 %s -o /dev/null

namespace std {
template <class T> struct alternative_traits;
struct alternative_info {
  decltype(^^int) info;
  bool empty = false;
  consteval alternative_info(decltype(^^int) info) : info(info) {}
};
}

struct Movable {
  Movable(Movable &&);
  ~Movable();
  bool valid() const;
};

bool consume(bool);
Movable make_movable();

// CHECK-LABEL: define{{.*}} i1 @moved_argument(
// CHECK: call{{.*}} @_ZN7MovableC1EOS_
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool moved_argument(Movable &value) {
  return consume(match(static_cast<Movable &&>(value),
                       case Movable owned if (owned.valid())));
}

// CHECK-LABEL: define{{.*}} i1 @temporary_argument(
// CHECK: call{{.*}} @_Z12make_movablev
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool temporary_argument() {
  return consume(match(make_movable(),
                       case const Movable &subject if (subject.valid())));
}

// CHECK-LABEL: define{{.*}} i1 @chained_argument(
// CHECK: call{{.*}} @_ZN7MovableC1EOS_
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool chained_argument(Movable &value) {
  return match(static_cast<Movable &&>(value),
               case Movable owned if (owned.valid())) && consume(true);
}

// CHECK-LABEL: define{{.*}} i1 @condition_argument(
// CHECK: call{{.*}} @_ZN7MovableC1EOS_
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: call{{.*}} @_Z7consumeb
extern "C" bool condition_argument(Movable &value) {
  if (match(static_cast<Movable &&>(value),
            case Movable owned if (owned.valid())))
    return consume(true);
  return consume(false);
}

struct Choice {
  unsigned active;
  Movable object;
  int integer;
};

template <> struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^Movable, ^^int};
  static constexpr bool has_residual_states = false;
  static constexpr unsigned index(const Choice &choice) noexcept {
    return choice.active;
  }
  template <unsigned I, class Self>
  static decltype(auto) get(Self &&choice) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(choice).object);
    else
      return (static_cast<Self &&>(choice).integer);
  }
};

bool valid(const Movable &value) { return value.valid(); }
bool valid(int value) { return value == 42; }

// CHECK-LABEL: define{{.*}} i1 @projected_argument(
// CHECK: call{{.*}} @_ZN7MovableC1EOS_
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool projected_argument(Choice &choice) {
  return consume(match(static_cast<Choice &&>(choice),
                       case { auto owned } if (valid(owned))));
}

extern bool next();

// CHECK-LABEL: define{{.*}} i1 @mixed_condition(
// CHECK: call void @_ZN7MovableC1EOS_(ptr {{.*}} %saved,
// CHECK: call void @_ZN7MovableC1EOS_(ptr {{.*}} %owned,
// CHECK: call{{.*}} @_Z4nextv(
// CHECK: store i1 false, ptr
// CHECK: call void @_ZN7MovableD1Ev(ptr {{.*}} %owned)
// CHECK: call{{.*}} @_ZNK7Movable5validEv(ptr {{.*}} %saved)
// CHECK: call{{.*}} @_Z7consumeb(
extern "C" bool mixed_condition(Movable &value) {
  if (case Movable saved = static_cast<Movable &&>(value) &&
      match(static_cast<Movable &&>(value), case Movable owned if (next())))
    return consume(saved.valid());
  return consume(false);
}

// CHECK-LABEL: define{{.*}} i1 @interleaved_condition(
// CHECK: call{{.*}} @_Z12make_movablev
// CHECK: call void @_ZN7MovableC1EOS_(ptr {{.*}} %saved,
// CHECK: call{{.*}} @_Z12make_movablev
// CHECK: call void @_ZN7MovableC1EOS_(ptr {{.*}} %owned,
// CHECK: call{{.*}} @_Z4nextv
// CHECK: call void @_ZN7MovableD1Ev(ptr {{.*}} %owned)
// CHECK: call{{.*}} @_ZNK7Movable5validEv(ptr {{.*}} %saved)
// CHECK: call{{.*}} @_Z7consumeb
extern "C" bool interleaved_condition(Movable &value) {
  if (match(make_movable(), case const Movable &) &&
      case Movable saved = static_cast<Movable &&>(value) &&
      match(make_movable(), case auto owned if (next())) &&
      case int number = 42 && next())
    return consume(saved.valid());
  return consume(false);
}

int with_test(int, bool);

// CHECK-LABEL: define{{.*}} i1 @nested_argument(
// CHECK: call{{.*}} @_ZN7MovableC1EOS_
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_Z9with_testib
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool nested_argument(Movable &value) {
  return consume(match(with_test(42, match(static_cast<Movable &&>(value),
                                          case Movable owned if (owned.valid()))),
                        case 42));
}

// CHECK-LABEL: define{{.*}} i1 @nested_temporary_argument(
// CHECK: call{{.*}} @_Z12make_movablev
// CHECK: call{{.*}} @_ZNK7Movable5validEv
// CHECK: call{{.*}} @_Z9with_testib
// CHECK: call{{.*}} @_Z7consumeb
// CHECK: call{{.*}} @_ZN7MovableD1Ev
// CHECK: ret i1
extern "C" bool nested_temporary_argument() {
  return consume(match(with_test(42, match(make_movable(),
                                          case const Movable &subject if (subject.valid()))),
                        case 42));
}
