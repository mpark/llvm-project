// RUN: %clang_cc1 -std=c++2d -fpattern-matching -verify -Wunused-value %s

void may_throw();
bool throwing_bool();
int throwing_int();

struct ThrowingCopy {
  ThrowingCopy(const ThrowingCopy&) noexcept(false);
  ~ThrowingCopy() noexcept(false);
};

extern ThrowingCopy throwing_copy;

static_assert(noexcept(0 match { case _ => 0; }));
static_assert(noexcept(0 match case _));
static_assert(noexcept(throwing_copy match case ThrowingCopy));

static_assert(!noexcept(throwing_int() match { case _ => 0; }));
static_assert(!noexcept(0 match {
  case throwing_int() => 0;
  case _ => 1;
}));
static_assert(!noexcept(0 match {
  case _ if (throwing_bool()) => 0;
  case _ => 1;
}));
static_assert(!noexcept(0 match { case _ => may_throw(); }));
static_assert(!noexcept(0 match case _ if (throwing_bool())));

void side_effects_are_observed(int &value) {
  throwing_int() match { case _ => 0; };
  value match {
    case ++value => 0;
    case _ => 1;
  };
  value match {
    case _ if (++value, true) => 0;
    case _ => 1;
  };
  value match { case _ => ++value; };
  value match case _ if ((++value, true));

  value match { case _ => value; }; // expected-warning {{expression result unused}}
  value match case _; // expected-warning {{expression result unused}}
}
