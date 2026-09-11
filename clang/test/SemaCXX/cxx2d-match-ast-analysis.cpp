// RUN: %clang_cc1 -std=c++2d -fpattern-matching -verify -Wunused-value %s

void may_throw();
bool throwing_bool();
int throwing_int();

static_assert(noexcept(match (0) { case _ => 0; }));
static_assert(noexcept(0 match case _));

static_assert(!noexcept(match (throwing_int()) { case _ => 0; }));
static_assert(!noexcept(match (0) {
  case throwing_int() => 0;
  case _ => 1;
}));
static_assert(!noexcept(match (0) {
  case _ if (throwing_bool()) => 0;
  case _ => 1;
}));
static_assert(!noexcept(match (0) { case _ => may_throw(); }));
static_assert(!noexcept(0 match case _ if (throwing_bool())));

void side_effects_are_observed(int &value) {
  match (throwing_int()) { case _ => 0; } // expected-warning {{expression result unused}}
  match (value) {
    case ++value => 0; // expected-warning {{expression result unused}}
    case _ => 1; // expected-warning {{expression result unused}}
  }
  match (value) {
    case _ if (++value, true) => 0; // expected-warning {{expression result unused}}
    case _ => 1; // expected-warning {{expression result unused}}
  }
  match (value) { case _ => ++value; }
  value match case _ if ((++value, true));

  match (value) { case _ => value; } // expected-warning {{expression result unused}}
  value match case _; // expected-warning {{expression result unused}}
}
