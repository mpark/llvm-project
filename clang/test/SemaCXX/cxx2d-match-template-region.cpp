// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only -verify \
// RUN:   -ast-dump -ast-dump-filter=singleton_projection_ %s \
// RUN:   | FileCheck %s --check-prefix=SINGLETON

#include "Inputs/std-coroutine.h"

namespace std {
template <class T> struct alternative_traits;

struct alternative_info {
  decltype(^^int) info;
  bool empty;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
}

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template <> struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^double};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Choice &value) noexcept {
    return value.active;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(value).integer);
    else
      return (static_cast<Self &&>(value).real);
  }
};

struct RepeatedChoice {
  unsigned active;
  int first;
  int second;
};

template <> struct std::alternative_traits<RepeatedChoice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const RepeatedChoice &value) noexcept {
    return value.active;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(value).first);
    else
      return (static_cast<Self &&>(value).second);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
};

template <> struct std::alternative_traits<MaybeInt> {
  static constexpr alternative_info alternatives[] = {{ {}, true }, ^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const MaybeInt &value) noexcept {
    return value.engaged;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    static_assert(I == 1);
    return (static_cast<Self &&>(value).value);
  }
};

template <class T> struct Maybe {
  bool engaged;
  T value;
};

template <class T> struct std::alternative_traits<Maybe<T>> {
  static constexpr alternative_info alternatives[] = {{ {}, true }, ^^T};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Maybe<T> &value) noexcept {
    return value.engaged;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    static_assert(I == 1);
    return (static_cast<Self &&>(value).value);
  }
};

struct EmptyChoice {};

template <> struct std::alternative_traits<EmptyChoice> {
  static constexpr alternative_info alternatives[] = {{ {}, true }};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const EmptyChoice &) noexcept { return 0; }
};

// Dependence counts advertised projectable entries, not distinct types or
// candidates remaining after viability and usefulness checks.
void multiple_projectable_states(Choice value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "discarded in each case instantiation");
    }
  }
  static_assert(false, "outside the match region"); // expected-error {{static assertion failed: outside the match region}}
}

void repeated_projectable_states(RepeatedChoice value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "repeated entries still introduce a region");
    }
  }
}

void singleton_projectable_state(MaybeInt value) {
  match (value) {
    case { auto payload } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "a singleton projection is not dependent"); // expected-error {{static assertion failed: a singleton projection is not dependent}}
    }
    case {} => ;
  }
}

constexpr bool singleton_projection_value(MaybeInt value) {
  return match (value, case { 42 });
}
// SINGLETON-LABEL: Dumping singleton_projection_value:
// SINGLETON: MatchTestExpr {{.*}} 'bool'{{$}}

constexpr bool singleton_projection_declaration(MaybeInt value) {
  return match (value, case { int payload } if (payload == 42));
}
// SINGLETON-LABEL: Dumping singleton_projection_declaration:
// SINGLETON: MatchTestExpr {{.*}} 'bool'{{$}}

constexpr bool singleton_projection_wildcard(MaybeInt value) {
  return match (value, case { _ });
}
// SINGLETON-LABEL: Dumping singleton_projection_wildcard:
// SINGLETON: MatchTestExpr {{.*}} 'bool'{{$}}

constexpr bool singleton_projection_nested(Maybe<MaybeInt> value) {
  return match (value, case { { 42 } });
}
// SINGLETON-LABEL: Dumping singleton_projection_nested:
// SINGLETON: MatchTestExpr {{.*}} 'bool'{{$}}

constexpr bool singleton_projection_nested_choice(Maybe<Choice> value) {
  return match (value, case { { int payload } } if (payload == 42));
}
// SINGLETON-LABEL: Dumping singleton_projection_nested_choice:
// SINGLETON: MatchTestExpr {{.*}} 'bool' instantiated{{$}}

static_assert(singleton_projection_value({true, 42}));
static_assert(!singleton_projection_value({true, 43}));
static_assert(!singleton_projection_value({false, 42}));
static_assert(singleton_projection_declaration({true, 42}));
static_assert(!singleton_projection_declaration({true, 43}));
static_assert(!singleton_projection_declaration({false, 42}));
static_assert(singleton_projection_wildcard({true, 42}));
static_assert(!singleton_projection_wildcard({false, 42}));
static_assert(singleton_projection_nested({true, {true, 42}}));
static_assert(!singleton_projection_nested({true, {true, 43}}));
static_assert(!singleton_projection_nested({true, {false, 42}}));
static_assert(!singleton_projection_nested({false, {true, 42}}));
static_assert(singleton_projection_nested_choice({true, {0, 42, 0.0}}));
static_assert(!singleton_projection_nested_choice({true, {1, 0, 42.0}}));
static_assert(!singleton_projection_nested_choice({false, {0, 42, 0.0}}));

constexpr bool nested_singleton_test(Maybe<MaybeInt> value) {
  return match (value, case { { auto payload } } if (payload == 42));
}

static_assert(nested_singleton_test({true, {true, 42}}));
static_assert(!nested_singleton_test({true, {true, 43}}));
static_assert(!nested_singleton_test({true, {false, 42}}));
static_assert(!nested_singleton_test({false, {true, 42}}));

void nested_singleton_region(Maybe<MaybeInt> value) {
  match (value) {
    case { { auto payload } } => {
      if constexpr (sizeof(payload) > 0)
        (void)payload;
      else
        static_assert(false, "nested singleton projections are not dependent"); // expected-error {{static assertion failed: nested singleton projections are not dependent}}
    }
    case _ => ;
  }
}

constexpr int nested_multiple_projectable_states(Maybe<Choice> value) {
  match (value) {
    case { { auto payload } } => {
      if constexpr (sizeof(payload) > 0)
        return static_cast<int>(payload);
      else
        static_assert(false, "the nested choice introduces a region");
    }
    case _ => return -1;
  }
}

static_assert(nested_multiple_projectable_states({true, {0, 17, 0.0}}) == 17);
static_assert(nested_multiple_projectable_states({true, {1, 0, 29.0}}) == 29);
static_assert(nested_multiple_projectable_states({false, {0, 17, 0.0}}) == -1);

void covered_multiple_projectable_states(Choice value) {
  match (value) {
    case { int } => ;
    case { double } => ;
    case { auto payload } => static_assert(false, "not instantiated"); // expected-warning {{match case is redundant}}
  }
}

void one_uncovered_projectable_state(Choice value) {
  match (value) {
    case { int } => ;
    case { auto payload } => {
      if constexpr (__is_same(decltype(payload), double))
        (void)payload;
      else
        static_assert(false, "coverage does not change source dependence");
    }
  }
}

template <class T>
concept Integer = __is_same(T, int);

void one_viable_projectable_state(Choice value) {
  match (value) {
    case { Integer auto payload } => {
      if constexpr (true)
        (void)payload;
      else
        static_assert(false, "constraints do not change source dependence");
    }
    case _ => ;
  }
}

void covered_repeated_projectable_states(RepeatedChoice value) {
  match (value) {
    case { int } => ;
    case { auto payload } => static_assert(false, "not instantiated"); // expected-warning {{match case is redundant}}
  }
}

void covered_singleton_projectable_state(MaybeInt value) {
  match (value) {
    case { int } => ;
    case {} => ;
    case { auto payload } => static_assert(false, "checked without instantiation"); // expected-error {{static assertion failed: checked without instantiation}} expected-warning {{match case is redundant}}
  }
}

void no_projectable_states(EmptyChoice value) {
  match (value) {
    case { auto payload } => ; // expected-error {{braced alternative pattern does not match any projectable state of 'EmptyChoice'}}
    case {} => ;
  }
}

template <class T>
concept NoCandidate = false;

bool no_viable_test_candidate(Choice value) {
  return match (value, case { NoCandidate auto payload }); // expected-error {{braced alternative pattern does not match any projectable state of 'Choice'}}
}

constexpr bool failing_test_guard(Choice value) {
  return match (value, case { auto payload } if (false));
}

static_assert(!failing_test_guard({0, 42, 0.0}));
static_assert(!failing_test_guard({1, 0, 2.0}));

void condition_region(Choice value) {
  if (case { auto payload } = value) {
    if constexpr (sizeof(payload) > 0)
      (void)payload;
    else
      static_assert(false, "discarded in the condition region");
  } else {
    static_assert(false, "else is outside the condition region"); // expected-error {{static assertion failed: else is outside the condition region}}
  }
}

void pattern_declaration_region(Choice value) {
  case { auto payload } = value;
  if constexpr (sizeof(payload) > 0)
    (void)payload;
  else
    static_assert(false, "discarded after resuming the region");
}

void singleton_condition(MaybeInt value) {
  if (case { auto payload } = value) {
    if constexpr (sizeof(payload) > 0)
      (void)payload;
    else
      static_assert(false, "a singleton condition is not dependent"); // expected-error {{static assertion failed: a singleton condition is not dependent}}
  }
}

constexpr int condition_chain(Choice first, RepeatedChoice second, int extra) {
  if (extra > 0 && case { auto payload } = first &&
      [] {
        if constexpr (true)
          return true;
        else
          static_assert(false, "discarded in a subsequent condition");
      }() && case { auto other } = second && other > 0) {
    if constexpr (sizeof(payload) > 0)
      return static_cast<int>(payload) + other + extra;
    else
      static_assert(false, "discarded in each condition instantiation");
  }
  return -1;
}

static_assert(condition_chain({0, 42, 0.0}, {0, 1, 2}, 3) == 46);
static_assert(condition_chain({1, 0, 2.0}, {1, 1, 2}, 3) == 7);
static_assert(condition_chain({1, 0, 2.0}, {1, 1, 0}, 3) == -1);

int deferred_assertion(Choice value) {
  return match (value) {
    case { auto payload } => // expected-note 2{{while instantiating this pattern match}}
      [] { static_assert(false); return 0; }(); // expected-error 2{{static assertion failed}} expected-note 2{{while substituting into a lambda expression here}}
  };
}

int nondependent_lookup(Choice value) {
  return match (value) {
    case { auto payload } => undeclared_name(); // expected-error {{use of undeclared identifier 'undeclared_name'}}
  };
}

constexpr int project(Choice value) {
  return match (value) {
    case { auto payload } => static_cast<int>(payload);
  };
}

static_assert(project({0, 42, 0}) == 42);
static_assert(project({1, 0, 24}) == 24);
