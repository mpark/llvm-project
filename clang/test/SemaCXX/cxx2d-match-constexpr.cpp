// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

template<int I>
constexpr int discards_unselected_handler() {
  return match constexpr (I) -> int {
    case 0 => 1;
    case _ => static_assert(false);
  };
}

static_assert(discards_unselected_handler<0>() == 1);

template<int I>
constexpr auto deduces_from_selected_handler() {
  return match constexpr (I) {
    case 0 => 1;
    case _ => "other";
  };
}

static_assert(deduces_from_selected_handler<0>() == 1);
static_assert(deduces_from_selected_handler<1>()[0] == 'o');

struct Pair {
  int first;
  int second;
};

constexpr int initializes_structural_bindings() {
  return match constexpr (Pair{42, 0}) {
    case [int first, 0] => first;
    case _ => -1;
  };
}

static_assert(initializes_structural_bindings() == 42);

constexpr auto immediate_integer = match constexpr (0) {
  case 0 => 1;
  case _ => "not selected";
};
static_assert(immediate_integer == 1);

constexpr auto immediate_string = match constexpr (1) {
  case 0 => 1;
  case _ => "selected";
};
static_assert(immediate_string[0] == 's');

int runtime_irrefutable(int value) {
  return match constexpr (value) {
    case auto&& selected => selected;
  };
}

int runtime_guard_with_constant_condition(int value) {
  return match constexpr (value) {
    case auto&& selected if (true) => selected;
    case _ => 0;
  };
}

int runtime_refutable(int value) { // expected-note {{declared here}}
  return match constexpr (value) { // expected-error {{constexpr if condition is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}} expected-note {{in call to '<expression body>'}}
    case 0 => 1;
    case _ => 2;
  };
}

int runtime_guard(int value) {
  return match constexpr (value) {
    case auto&& selected if (selected == 0) => 1; // expected-error {{constexpr if condition is not a constant expression}} expected-note {{read of non-constexpr variable 'selected' is not allowed in a constant expression}} expected-note {{declared here}}
    case _ => 0;
  };
}
