// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

namespace std {
template<class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
}

struct Sized {
  int value;

  constexpr int size() const { return value; }
};

template<class T>
constexpr int whole_subject(T value) {
  return value match {
    case int i => i;
    case auto&& other => other.size();
  };
}

static_assert(whole_subject(42) == 42);
static_assert(whole_subject(Sized{7}) == 7);

struct Choice {
  unsigned state;
  int integer;
  Sized sized;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^Sized
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).integer);
    else
      return (static_cast<Self&&>(choice).sized);
  }
};

constexpr int closed_choice(Choice choice) {
  return choice match {
    case { int i } => i;
    case { auto&& other } => other.size();
  };
}

static_assert(closed_choice({0, 4, {}}) == 4);
static_assert(closed_choice({1, 0, {6}}) == 6);

struct ChoicePair {
  Choice first;
  Choice second;
};

constexpr int nested_choices(ChoicePair choices) {
  return choices match {
    case [{ int i }, _] => i;
    case [_, { int i }] => i;
    case [{ auto&& first }, { auto&& second }] =>
        first.size() + second.size();
  };
}

static_assert(nested_choices({{0, 1, {}}, {1, 0, {2}}}) == 1);
static_assert(nested_choices({{1, 0, {2}}, {0, 3, {}}}) == 3);
static_assert(nested_choices({{1, 0, {2}}, {1, 0, {3}}}) == 5);

struct One {
  int value;
};

template<>
struct std::alternative_traits<One> {
  static constexpr alternative_info alternatives[] = {
    ^^int
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const One&) noexcept { return 0; }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& self) {
    return (static_cast<Self&&>(self).value);
  }
};

int one_state(One one) {
  return one match {
    case { int i } => i;
    case { auto&& other } => other.no_such_member(); // expected-error {{match case is redundant}}
  };
}

template<class T>
int guarded_arm_does_not_close(T value) {
  return value match {
    case int i if (i > 0) => i;
    case auto&& other => other.no_such_member(); // expected-error {{member reference base type 'int' is not a structure or union}}
  };
}

int use_guarded_arm() {
  return guarded_arm_does_not_close(1); // expected-note {{in instantiation of function template specialization 'guarded_arm_does_not_close<int>' requested here}}
}

template<class T>
int value_arm_does_not_close(T value) {
  return value match {
    case 0 => 0;
    case auto&& other => other.no_such_member(); // expected-error {{member reference base type 'int' is not a structure or union}}
  };
}

int use_value_arm() {
  return value_arm_does_not_close(0); // expected-note {{in instantiation of function template specialization 'value_arm_does_not_close<int>' requested here}}
}

template<class T>
int refutable_arms_do_not_collect(T value) {
  return value match {
    case true => 1;
    case false => 0;
    case auto&& other => other.no_such_member(); // expected-error {{member reference base type 'bool' is not a structure or union}}
  };
}

int use_refutable_arms() {
  return refutable_arms_do_not_collect(true); // expected-note {{in instantiation of function template specialization 'refutable_arms_do_not_collect<bool>' requested here}}
}

struct Base {
  virtual ~Base();
};
struct Derived : Base {};

template<class T>
int dynamic_cast_arm_does_not_close(T& value) {
  return value match {
    case Derived& derived => 1;
    case auto&& other => other.no_such_member(); // expected-error {{no member named 'no_such_member' in 'Base'}}
  };
}

int use_dynamic_cast_arm(Base& base) {
  return dynamic_cast_arm_does_not_close(base); // expected-note {{in instantiation of function template specialization 'dynamic_cast_arm_does_not_close<Base>' requested here}}
}
