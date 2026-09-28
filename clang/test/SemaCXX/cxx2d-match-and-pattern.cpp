// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s -verify

constexpr int boolean(bool value) {
  return match (value) {
    case true and true => 1;
    case false => 0;
  };
}

static_assert(boolean(true) == 1);
static_assert(boolean(false) == 0);

constexpr int precedence(bool value) {
  return match (value) {
    case false or true and true => 1;
  };
}

static_assert(precedence(false) == 1);
static_assert(precedence(true) == 1);

constexpr bool alternative_token(bool value) {
  return match(value, case true and true);
}

static_assert(alternative_token(true));
static_assert(!alternative_token(false));

constexpr int binding(int value) {
  return match (value) {
    case int x and _ => x;
  };
}

static_assert(binding(42) == 42);

constexpr int binding_from_or(int value) {
  return match (value) {
    case (int x and 0) or (int x and 1) => x;
    case _ => -1;
  };
}

static_assert(binding_from_or(0) == 0);
static_assert(binding_from_or(1) == 1);
static_assert(binding_from_or(2) == -1);

void binding_not_visible_in_pattern(int value) {
  match (value) {
    case int x and x =>; // expected-error {{pattern binding 'x' cannot be used within the pattern that introduces it}}
    case _ =>;
  }
}

constexpr int rvalue_reference() {
  return match (42) {
    case int&& value => value;
  };
}

static_assert(rvalue_reference() == 42);

struct Counted {
  int value;
  int *copies;

  constexpr Counted(int value, int &copies) : value(value), copies(&copies) {}
  constexpr Counted(const Counted &other)
      : value(other.value), copies(other.copies) {
    ++*copies;
  }
};

struct copied_once {
  constexpr bool operator()(const Counted &value) const {
    return *value.copies == 1;
  }
};

inline constexpr copied_once was_copied_once;

constexpr bool declaration_before_rhs() {
  int copies = 0;
  Counted value(1, copies);
  return match(value, case Counted copy and was_copied_once);
}

static_assert(declaration_before_rhs());

struct Lifetime {
  int *destructions;

  constexpr Lifetime(int &destructions) : destructions(&destructions) {}
  constexpr Lifetime(const Lifetime &) = default;
  constexpr ~Lifetime() { ++*destructions; }
};

struct rejects_lifetime {
  constexpr bool operator()(const Lifetime &) const { return false; }
};

inline constexpr rejects_lifetime rejects;

constexpr int cleanup_before_next_case() {
  int destructions = 0;
  Lifetime value(destructions);
  return match (value) {
    case Lifetime copy and rejects => -1;
    case _ => destructions;
  };
}

static_assert(cleanup_before_next_case() == 1);

struct less_than {
  int limit;

  constexpr bool operator()(int value) const { return value < limit; }
};

inline constexpr less_than ten{10};
inline constexpr less_than five{5};

constexpr int short_circuit(int value) {
  return match (value) {
    case ten and five => 1;
    case ten => 2;
    case _ => 3;
  };
}

static_assert(short_circuit(3) == 1);
static_assert(short_circuit(7) == 2);
static_assert(short_circuit(12) == 3);

struct Pair {
  int first;
  int second;
};

constexpr bool nested(Pair pair) {
  return match(pair, case [0, _] and [_, 1]);
}

static_assert(nested({0, 1}));
static_assert(!nested({0, 2}));
static_assert(!nested({2, 1}));

struct NotAnInt {};

template<class T>
constexpr int dependent(T value) {
  return match (value) {
    case (int) and 0 => 1;
    case _ => 0;
  };
}

static_assert(dependent(0) == 1);
static_assert(dependent(1) == 0);
static_assert(dependent(NotAnInt{}) == 0);

void strict_viability(NotAnInt value) {
  (void)match(value, case (int) and 0); // expected-error {{type pattern of type 'int' is not an exact match for subject of type 'NotAnInt'}}
}

void impossible(bool value) {
  match (value) {
    case false and true =>; // expected-error {{match case can never match a subject of type 'bool'}}
    case _ =>;
  }
}

struct Widget {
  Widget();
  Widget(const Widget &) = delete;
  Widget(Widget &&);
};

void nontrivial_move(Widget value) {
  match (static_cast<Widget&&>(value)) {
    case Widget moved and _ =>; // expected-error {{declaration pattern of type 'Widget' invokes a non-trivial move constructor before a later operand of an and-pattern}}
  }
}

void no_double_move(Widget value) {
  match (static_cast<Widget&&>(value)) {
    case Widget first and Widget second =>; // expected-error {{declaration pattern of type 'Widget' invokes a non-trivial move constructor before a later operand of an and-pattern}}
  }
}
