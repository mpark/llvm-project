// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

template<int I>
constexpr int discards_unselected_handler() {
  return I match constexpr -> int {
    case 0 => 1;
    case _ => static_assert(false);
  };
}

static_assert(discards_unselected_handler<0>() == 1);

template<int I>
constexpr auto deduces_from_selected_handler() {
  return I match constexpr {
    case 0 => 1;
    case _ => "other";
  };
}

static_assert(deduces_from_selected_handler<0>() == 1);
static_assert(deduces_from_selected_handler<1>()[0] == 'o');

constexpr auto immediate_integer = 0 match constexpr {
  case 0 => 1;
  case _ => "not selected";
};
static_assert(immediate_integer == 1);

constexpr auto immediate_string = 1 match constexpr {
  case 0 => 1;
  case _ => "selected";
};
static_assert(immediate_string[0] == 's');

int runtime_irrefutable(int value) {
  return value match constexpr {
    case auto&& selected => selected;
  };
}

int runtime_guard_with_constant_condition(int value) {
  return value match constexpr {
    case auto&& selected if (true) => selected;
    case _ => 0;
  };
}

int runtime_refutable(int value) {
  return value match constexpr { // expected-error {{constexpr if condition is not a constant expression}} expected-note {{read of non-constexpr variable '' is not allowed in a constant expression}} expected-note {{declared here}}
    case 0 => 1;
    case _ => 2;
  };
}

int runtime_guard(int value) {
  return value match constexpr {
    case auto&& selected if (selected == 0) => 1; // expected-error {{constexpr if condition is not a constant expression}} expected-note {{read of non-constexpr variable 'selected' is not allowed in a constant expression}} expected-note {{declared here}}
    case _ => 0;
  };
}
