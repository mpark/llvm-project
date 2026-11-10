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

constexpr int enclosing_parameter_and_nested_lambda(Choice value, int extra) {
  return match (value) {
    case { auto payload } => [&]<class T>(T argument) {
      return static_cast<int>(argument) + extra;
    }(payload);
  };
}

static_assert(enclosing_parameter_and_nested_lambda({0, 42, 0.0}, 3) == 45);
static_assert(enclosing_parameter_and_nested_lambda({1, 0, 2.0}, 3) == 5);

constexpr int nested_generic_match(Choice value) {
  return match (value) {
    case { auto payload } => [payload]<class T>(T extra) {
      return match (payload) -> int {
        case 0 => extra;
        case auto copy => static_cast<int>(copy) + extra;
      };
    }(3);
  };
}

static_assert(nested_generic_match({0, 42, 0.0}) == 45);
static_assert(nested_generic_match({1, 0, 2.0}) == 5);
static_assert(nested_generic_match({0, 0, 0.0}) == 3);

struct EnclosingObject {
  int extra;

  constexpr int classify(Choice value) const {
    return match (value) {
      case { auto payload } => [this](auto argument) {
        return static_cast<int>(argument) + extra;
      }(payload);
    };
  }
};

static_assert(EnclosingObject{3}.classify({0, 42, 0.0}) == 45);
static_assert(EnclosingObject{3}.classify({1, 0, 2.0}) == 5);

constexpr int enclosing_lambda(Choice value, int extra) {
  return [=] {
    return match (value) {
      case { auto payload } => static_cast<int>(payload) + extra;
    };
  }();
}

static_assert(enclosing_lambda({0, 42, 0.0}, 3) == 45);
static_assert(enclosing_lambda({1, 0, 2.0}, 3) == 5);

void failed_case_instantiation(Choice value) {
  match (value) {
    case { auto payload } => static_assert(__is_same(decltype(payload), int), "requires int"); // expected-error {{static assertion failed}} expected-note {{while instantiating this pattern match}}
  }
}

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

constexpr bool guarded_test(Choice value) {
  return match(value, case { auto payload } if ([] {
    if constexpr (true)
      return true;
    else
      static_assert(false, "discarded in each guard instantiation");
  }()));
}

static_assert(guarded_test({0, 42, 0.0}));
static_assert(guarded_test({1, 0, 2.0}));

constexpr int condition_loop(RepeatedChoice value) {
  int result = 0;
  for (; case { auto &payload } = value; ++payload) {
    if constexpr (sizeof(payload) > 0) {
      if (payload == 3)
        break;
      if (payload == 0)
        continue;
      result += payload;
    } else {
      static_assert(false, "discarded in the loop region");
    }
  }
  return result;
}

static_assert(condition_loop({0, 0, 0}) == 3);
static_assert(condition_loop({1, 0, 1}) == 3);

struct Pair {
  int first;
  int second;
};

struct Triple {
  int first;
  int second;
  int third;
};

constexpr int direct_declaration_pack(Pair value) {
  match (value) {
    case [auto&& ...elements] => {
      if constexpr (sizeof...(elements) == 2)
        return (... + elements);
      else
        static_assert(false, "a declaration pack introduces a region");
    }
  }
}

static_assert(direct_declaration_pack({1, 2}) == 3);

constexpr int direct_structured_binding_pack(Pair value) {
  match (value) {
    case auto &[...elements] => {
      if constexpr (sizeof...(elements) == 2)
        return (... + elements);
      else
        static_assert(false, "a structured binding pack introduces a region");
    }
  }
}

static_assert(direct_structured_binding_pack({1, 2}) == 3);

constexpr int namespace_declaration_pack = match (Pair{20, 22}) {
  case [auto&& ...elements] => (... + elements);
};
static_assert(namespace_declaration_pack == 42);

constexpr int namespace_structured_binding_pack = match (Pair{20, 22}) {
  case auto [...elements] => (... + elements);
};
static_assert(namespace_structured_binding_pack == 42);

constexpr bool direct_pack_guard(Pair value) {
  return match (value, case [auto&& ...elements] if ((... + elements) == 3));
}

static_assert(direct_pack_guard({1, 2}));
static_assert(!direct_pack_guard({3, 4}));

void region_does_not_extend_to_next_case(Pair value) {
  match (value) {
    case [0, auto&& ...elements] => {
      if constexpr (sizeof...(elements) == 1)
        (void)(... + elements);
      else
        static_assert(false, "discarded in the pack's region");
    }
    case [auto first, auto second] => {
      if constexpr (true)
        (void)(first + second);
      else
        static_assert(false, "the next case is not dependent"); // expected-error {{static assertion failed: the next case is not dependent}}
    }
  }
}

struct NestedProduct {
  Pair pair;
  int last;
};

constexpr int nested_decomposition_in_region(Choice value,
                                             NestedProduct product) {
  return match (value, product) {
    case [{ auto payload }, const auto &[[first, second], last]] =>
      static_cast<int>(payload) + first + second + last;
  };
}

static_assert(nested_decomposition_in_region({0, 10, 0.0}, {{1, 2}, 3}) == 16);
static_assert(nested_decomposition_in_region({1, 0, 2.0}, {{1, 2}, 3}) == 8);

struct ProductChoice {
  unsigned active;
  Pair pair;
  Triple triple;
};

template <> struct std::alternative_traits<ProductChoice> {
  static constexpr alternative_info alternatives[] = {^^Pair, ^^Triple};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const ProductChoice &value) noexcept {
    return value.active;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&value) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(value).pair);
    else
      return (static_cast<Self &&>(value).triple);
  }
};

constexpr int structured_binding_pack_in_region(ProductChoice value) {
  match (value) {
    case { auto &payload } => {
      auto &[...elements] = payload;
      return (... + elements);
    }
  }
}

static_assert(structured_binding_pack_in_region({0, {1, 2}, {3, 4, 5}}) == 3);
static_assert(structured_binding_pack_in_region({1, {1, 2}, {3, 4, 5}}) == 12);

constexpr int or_structured_binding_pack(ProductChoice value) {
  return match (value) {
    case { Pair: auto &[...elements] } or
         { Triple: auto &[...elements] } => (... + elements);
  };
}

static_assert(or_structured_binding_pack({0, {1, 2}, {3, 4, 5}}) == 3);
static_assert(or_structured_binding_pack({1, {1, 2}, {3, 4, 5}}) == 12);

constexpr int declaration_pack_in_region(ProductChoice value) {
  match (value) {
    case { [auto&& ...elements] } => {
      if constexpr (sizeof...(elements) > 0)
        return (... + elements);
      else
        static_assert(false, "discarded after expanding the declaration pack");
    }
  }
}

static_assert(declaration_pack_in_region({0, {1, 2}, {3, 4, 5}}) == 3);
static_assert(declaration_pack_in_region({1, {1, 2}, {3, 4, 5}}) == 12);

constexpr long do_expression_in_region(Choice value) {
  return match (value) -> long {
    case { auto payload } => do {
      if constexpr (__is_same(decltype(payload), int))
        do_return 17;
      else
        do_return 29L;
    };
  };
}

static_assert(do_expression_in_region({0, 0, 0.0}) == 17);
static_assert(do_expression_in_region({1, 0, 0.0}) == 29);

void variadic_function(Choice value, int last, ...) {
  __builtin_va_list arguments;
  match (value) {
    case { auto payload } => {
      __builtin_va_start(arguments, last);
      (void)__builtin_va_arg(arguments, int);
      __builtin_va_end(arguments);
      (void)payload;
    }
  }
}

void jump_between_handlers(Choice value) {
  match (value) {
    case { int } => goto other; // expected-error {{cannot jump from this goto statement to its label}}
    case { double } => { // expected-note {{jump enters a match handler}}
    other:
      ;
    }
  }
}

void jump_into_handler(Choice value) {
  goto inside; // expected-error {{cannot jump from this goto statement to its label}}
  match (value) {
    case { auto payload } => { // expected-note {{jump enters a match handler}}
    inside:
      (void)payload;
    }
  }
}

struct Task {
  struct promise_type {
    Task get_return_object();
    std::suspend_never initial_suspend() noexcept;
    std::suspend_never final_suspend() noexcept;
    void return_void();
    void unhandled_exception();
  };
};

Task coroutine_function(Choice value) {
  match (value) {
    case { auto payload } => {
      co_await std::suspend_never{};
      (void)payload;
    }
  }
  co_return;
}
