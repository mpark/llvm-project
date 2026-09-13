// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s -verify

struct AtLeast {
  int threshold;

  constexpr bool operator()(int value) const {
    return value >= threshold;
  }
};

inline constexpr AtLeast at_least_three{3};

static_assert(match(3, case at_least_three));
static_assert(match(4, case at_least_three));
static_assert(!match(2, case at_least_three));

constexpr int select_at_least_three(int value) {
  return match (value) {
    case at_least_three => 1;
    case _ => 0;
  };
}

static_assert(select_at_least_three(3) == 1);
static_assert(select_at_least_three(2) == 0);

constexpr bool condition_at_least_three(int value) {
  if (case at_least_three = value)
    return true;
  return false;
}

static_assert(condition_at_least_three(3));
static_assert(!condition_at_least_three(2));

constexpr int nested_predicate(int value) {
  int *pointer = &value;
  return match (pointer) {
    case { at_least_three } => 1;
    case _ => 0;
  };
}

static_assert(nested_predicate(3) == 1);
static_assert(nested_predicate(2) == 0);

constexpr bool is_even(int value) {
  return value % 2 == 0;
}

static_assert(match(4, case is_even));
static_assert(!match(3, case is_even));

consteval bool immediate_is_even(int value) {
  return value % 2 == 0;
}

static_assert(match(4, case immediate_is_even));
static_assert(!match(3, case immediate_is_even));

static_assert(match(4, case auto([](int value) constexpr {
  return value % 2 == 0;
})));
static_assert(!match(3, case auto([](int value) constexpr {
  return value % 2 == 0;
})));

struct BooleanLike {
  constexpr explicit operator bool() const { return true; }
};

struct ReturnsBooleanLike {
  constexpr BooleanLike operator()(int) const { return {}; }
};

inline constexpr ReturnsBooleanLike returns_boolean_like;
static_assert(match(0, case returns_boolean_like));

struct NotBoolean {};

struct EqualityResultIsNotBoolean {
  friend constexpr NotBoolean operator==(int,
                                         EqualityResultIsNotBoolean) {
    return {};
  }

  constexpr bool operator()(int) const { return true; }
};

inline constexpr EqualityResultIsNotBoolean equality_result_is_not_boolean;
static_assert(match(0, case equality_result_is_not_boolean));

struct EqualityWins {
  int value;

  constexpr operator int() const { return value; }

  template<class T>
  constexpr bool operator()(const T&) const {
    static_assert(sizeof(T) == 0, "predicate must not be instantiated");
    return true;
  }
};

inline constexpr EqualityWins five{5};

static_assert(match(5, case five));
static_assert(!match(6, case five));

struct OnlyInt {
  constexpr bool operator()(int) const { return true; }
};

inline constexpr OnlyInt only_int;

template<class T>
constexpr int dependent(T value) {
  return match (value) {
    case only_int => 1;
    case _ => 0;
  };
}

struct Unrelated {};

static_assert(dependent(1) == 1);
static_assert(dependent(Unrelated{}) == 0);

int predicate_is_not_exhaustive(int value) {
  return match (value) { // expected-error {{match expression is not exhaustive; example of a missing case: 0}}
    case at_least_three => 1;
  };
}

struct NotTestable {};
inline constexpr NotTestable not_testable;

bool invalid_expression_pattern(int value) {
  return match(value, case not_testable); // expected-error {{expression pattern of type 'const NotTestable' is neither equality-comparable with a subject of type 'int' nor callable with it}}
}

struct RuntimePredicate {
  int threshold;
  constexpr bool operator()(int value) const { return value >= threshold; }
};

bool non_constant_pattern(int value, RuntimePredicate predicate) { // expected-note {{declared here}}
  return match(value, case predicate); // expected-error {{pattern is not a constant expression}} expected-note {{reference to 'predicate' is not a constant expression}}
}

bool non_constant_condition(int value, RuntimePredicate predicate) { // expected-note {{declared here}}
  if (case predicate = value) // expected-error {{pattern is not a constant expression}} expected-note {{reference to 'predicate' is not a constant expression}}
    return true;
  return false;
}

int non_constant_selection(int value, RuntimePredicate predicate) { // expected-note {{declared here}}
  return match (value) {
    case predicate => 1; // expected-error {{pattern is not a constant expression}} expected-note {{reference to 'predicate' is not a constant expression}}
    case _ => 0;
  };
}
