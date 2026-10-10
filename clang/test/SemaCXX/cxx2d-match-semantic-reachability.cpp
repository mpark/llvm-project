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
  return match (value) {
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
  return match (choice) {
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
  return match (choices) {
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
  return match (one) {
    case { int i } => i;
    // expected-error@+2 {{member reference base type 'int' is not a structure or union}}
    // expected-error@+1 {{match case is redundant}}
    case { auto&& other } => other.no_such_member();
  };
}

template<class T>
int guarded_arm_does_not_close(T value) {
  return match (value) {
    case int i if (i > 0) => i;
    case auto&& other => other.no_such_member(); // expected-error {{member reference base type 'int' is not a structure or union}}
  };
}

int use_guarded_arm() {
  return guarded_arm_does_not_close(1); // expected-note {{in instantiation of function template specialization 'guarded_arm_does_not_close<int>' requested here}}
}

template<class T>
int value_arm_does_not_close(T value) {
  return match (value) {
    case 0 => 0;
    case auto&& other => other.no_such_member(); // expected-error {{member reference base type 'int' is not a structure or union}}
  };
}

int use_value_arm() {
  return value_arm_does_not_close(0); // expected-note {{in instantiation of function template specialization 'value_arm_does_not_close<int>' requested here}}
}

template<class T>
constexpr int refutable_arms_collectively_close(T value) {
  return match (value) {
    case true => 1;
    case false => 0;
    case auto&& other => other.size();
  };
}

static_assert(refutable_arms_collectively_close(true) == 1);
static_assert(refutable_arms_collectively_close(false) == 0);
static_assert(refutable_arms_collectively_close(Sized{5}) == 5);

struct BoolChoice {
  unsigned state;
  bool boolean;
  Sized sized;
};

template<>
struct std::alternative_traits<BoolChoice> {
  static constexpr alternative_info alternatives[] = {
    ^^bool, ^^Sized
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const BoolChoice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).boolean);
    else
      return (static_cast<Self&&>(choice).sized);
  }
};

constexpr int collectively_closed_alternative(BoolChoice choice) {
  return match (choice) {
    case { true } => 1;
    case { false } => 0;
    case { auto&& other } => other.size();
  };
}

static_assert(collectively_closed_alternative({0, true, {}}) == 1);
static_assert(collectively_closed_alternative({0, false, {}}) == 0);
static_assert(collectively_closed_alternative({1, false, {7}}) == 7);

template<class T>
struct CandidateConstraint {
  static_assert(!__is_same(T, bool),
                "covered alternative candidate was instantiated");
  static constexpr bool value = __is_same(T, Sized);
};

template<class T>
concept UsefulCandidate = CandidateConstraint<T>::value;

constexpr int skip_covered_alternative_candidate(BoolChoice choice) {
  return match (choice) {
    case { true } => 1;
    case { false } => 0;
    case { UsefulCandidate: auto&& other } => other.size();
  };
}

static_assert(skip_covered_alternative_candidate({0, true, {}}) == 1);
static_assert(skip_covered_alternative_candidate({0, false, {}}) == 0);
static_assert(skip_covered_alternative_candidate({1, false, {8}}) == 8);

struct NestedBoolChoice {
  BoolChoice choice;
  int marker;
};

constexpr int skip_nested_covered_candidate(NestedBoolChoice nested) {
  return match (nested) {
    case [{ true }, _] => 1;
    case [{ false }, _] => 0;
    case [{ UsefulCandidate: auto&& other }, _] => other.size();
  };
}

static_assert(skip_nested_covered_candidate({{0, true, {}}, 0}) == 1);
static_assert(skip_nested_covered_candidate({{0, false, {}}, 0}) == 0);
static_assert(skip_nested_covered_candidate({{1, false, {9}}, 0}) == 9);

struct TwoBoolChoices {
  BoolChoice first;
  BoolChoice second;
};

constexpr int skip_covered_first_field(TwoBoolChoices choices) {
  return match (choices) {
    case [{ true }, _] => 1;
    case [{ false }, _] => 0;
    case [{ UsefulCandidate: auto&& other }, _] => other.size();
  };
}

static_assert(skip_covered_first_field({{0, true, {}}, {0, false, {}}}) == 1);
static_assert(skip_covered_first_field({{0, false, {}}, {1, false, {}}}) == 0);
static_assert(skip_covered_first_field({{1, false, {10}}, {0, true, {}}}) == 10);

constexpr int skip_covered_second_field(TwoBoolChoices choices) {
  return match (choices) {
    case [_, { true }] => 1;
    case [_, { false }] => 0;
    case [_, { UsefulCandidate: auto&& other }] => other.size();
  };
}

static_assert(skip_covered_second_field({{0, false, {}}, {0, true, {}}}) == 1);
static_assert(skip_covered_second_field({{1, false, {}}, {0, false, {}}}) == 0);
static_assert(skip_covered_second_field({{0, true, {}}, {1, false, {11}}}) == 11);

int coverage_is_path_sensitive(TwoBoolChoices choices) {
  return match (choices) {
    case [{ true }, _] => 1;
    case [{ false }, _] => 0;
    case [_, { auto&& other }] => other.size(); // expected-error {{member reference base type 'bool' is not a structure or union}}
  };
}

int partially_covered_alternative(BoolChoice choice) {
  return match (choice) {
    case { true } => 1;
    case { auto&& other } => other.size(); // expected-error {{member reference base type 'bool' is not a structure or union}}
  };
}

constexpr int precisely_redundant_candidate(BoolChoice choice) {
  return match (choice) {
    case { true } => 1;
    case { true } => 2; // expected-error {{match case is redundant}}
    case { false } => 0;
    case { auto&& other } => other.size();
  };
}

struct Base {
  virtual ~Base();
};
struct Derived : Base {};

template<class T>
int dynamic_cast_arm_does_not_close(T& value) {
  return match (value) {
    case Derived& derived => 1;
    case auto&& other => other.no_such_member(); // expected-error {{no member named 'no_such_member' in 'Base'}}
  };
}

int use_dynamic_cast_arm(Base& base) {
  return dynamic_cast_arm_does_not_close(base); // expected-note {{in instantiation of function template specialization 'dynamic_cast_arm_does_not_close<Base>' requested here}}
}
