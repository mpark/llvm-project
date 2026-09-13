// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

template <int Value>
  requires(match(Value, case 0 || 1))
constexpr bool namespace_constraint() {
  return true;
}

template <int Value>
concept accepts_namespace_constraint = requires { namespace_constraint<Value>(); };

static_assert(namespace_constraint<0>());
static_assert(namespace_constraint<1>());
static_assert(!accepts_namespace_constraint<2>);

struct constrained_members {
  template <int Value>
    requires(match(Value, case 0 || 1))
  static constexpr bool accepts_zero_or_one() {
    return true;
  }
};

template <int Value>
concept accepts_member_constraint =
    requires { constrained_members::accepts_zero_or_one<Value>(); };

static_assert(constrained_members::accepts_zero_or_one<0>());
static_assert(constrained_members::accepts_zero_or_one<1>());
static_assert(!accepts_member_constraint<2>);

// expected-no-diagnostics
