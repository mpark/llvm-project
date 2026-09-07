// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s -verify

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

enum class Direction { north, south, east, west };

constexpr bool is_vertical(Direction direction) {
  return direction match case Direction::north || Direction::south;
}

static_assert(is_vertical(Direction::north));
static_assert(is_vertical(Direction::south));
static_assert(!is_vertical(Direction::east));

constexpr int exhaustive_bool(bool value) {
  return value match {
    case false || true => 1;
  };
}

static_assert(exhaustive_bool(false) == 1);
static_assert(exhaustive_bool(true) == 1);

constexpr int redundant_alternative(bool value) {
  return value match {
    case false || false => 0; // expected-error {{or-pattern alternative is redundant}}
    case true => 1;
  };
}

constexpr int dominated_alternative(bool value) {
  return value match {
    case _ || true => 0; // expected-error {{or-pattern alternative is redundant}}
  };
}

struct Pair {
  int first;
  int second;
};

constexpr bool nested(Pair pair) {
  return pair match {
    case [0 || 1, 2 || 3] => true;
    case _ => false;
  };
}

static_assert(nested({0, 2}));
static_assert(nested({1, 3}));
static_assert(!nested({2, 2}));

constexpr bool parenthesized_expression(bool value) {
  return value match {
    case (false || true) => true;
    case _ => false;
  };
}

static_assert(parenthesized_expression(true));
static_assert(!parenthesized_expression(false));

constexpr bool logical_and_remains_an_expression(bool value) {
  return value match {
    case true && false => true;
    case _ => false;
  };
}

static_assert(logical_and_remains_an_expression(false));

constexpr int bind_from_either_position(Pair pair) {
  return pair match {
    case [0, int value] || [int value, 0] => value;
    case _ => -1;
  };
}

static_assert(bind_from_either_position({0, 3}) == 3);
static_assert(bind_from_either_position({4, 0}) == 4);

constexpr int guard_runs_once(Pair pair) {
  int guards = 0;
  return pair match {
    case [0, int value] || [int value, 0]
        if (++guards, false) => value;
    case _ => guards;
  };
}

static_assert(guard_runs_once({0, 0}) == 1);

constexpr bool guarded_test(Pair pair) {
  return pair match case [0, int value] || [int value, 0] if (value > 2);
}

static_assert(guarded_test({0, 3}));
static_assert(guarded_test({4, 0}));
static_assert(!guarded_test({0, 1}));

constexpr int case_condition(Pair pair) {
  if (case [0, int value] || [int value, 0] = pair)
    return value;
  return -1;
}

static_assert(case_condition({0, 3}) == 3);
static_assert(case_condition({4, 0}) == 4);
static_assert(case_condition({4, 5}) == -1);

struct Triple {
  int first;
  int second;
  int third;
};

constexpr int bind_pack_from_either_end(Triple triple) {
  return triple match {
    case [0, auto&& ...values] || [auto&& ...values, 0] =>
        int(sizeof...(values)) + (... + values);
    case _ => -1;
  };
}

static_assert(bind_pack_from_either_end({0, 2, 3}) == 7);
static_assert(bind_pack_from_either_end({2, 3, 0}) == 7);

constexpr int bind_empty_pack(Pair pair) {
  return pair match {
    case [0, auto&& ...values, 1] || [1, auto&& ...values, 0] =>
        int(sizeof...(values));
    case _ => -1;
  };
}

static_assert(bind_empty_pack({0, 1}) == 0);
static_assert(bind_empty_pack({1, 0}) == 0);

void pack_must_be_a_direct_decomposition_element(Pair pair) {
  match(pair, case [auto&& ...values || _]); // expected-error {{pattern pack must be a direct element of a decomposition pattern}}
  match(pair, case [auto&& ... || _]); // expected-error {{pattern pack must be a direct element of a decomposition pattern}}
  match(pair, case [... || _]); // expected-error {{pattern pack must be a direct element of a decomposition pattern}}
}

template <int... Values>
void expression_pack_must_be_a_direct_decomposition_element(Pair pair) {
  match(pair, case [Values... || 0]); // expected-error {{pattern pack must be a direct element of a decomposition pattern}}
}

struct NestedPair {
  Pair pair;
  int other;
};

constexpr int nested_binding(NestedPair value) {
  return value match {
    case [[0, int selected] || [int selected, 0], _] => selected;
    case _ => -1;
  };
}

static_assert(nested_binding({{0, 3}, 4}) == 3);
static_assert(nested_binding({{4, 0}, 5}) == 4);

int mismatched_bindings(Pair pair) {
  return pair match {
    case [0, int x] || [int y, 0] => x; // expected-error {{all alternatives in an or-pattern must introduce the same bindings}} expected-note {{binding interface established by this alternative}}
    case _ => 0;
  };
}

int missing_binding(Pair pair) {
  return pair match {
    case [0, int value] || [0, 0] => value; // expected-error {{all alternatives in an or-pattern must introduce the same bindings}} expected-note {{binding interface established by this alternative}}
    case _ => 0;
  };
}

int mismatched_pack_binding(Pair pair) {
  return pair match {
    case [0, auto&& ...value] || [auto&& value, 0] => 0; // expected-error {{all alternatives in an or-pattern must introduce the same bindings}} expected-note {{binding interface established by this alternative}}
    case _ => 0;
  };
}

struct Left {};
struct Right {};
struct Other {};

struct Choice {
  unsigned state;
  Left left;
  Right right;
  Other other;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {
      ^^Left, ^^Right, ^^Other};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<unsigned I, class Self>
  static constexpr decltype(auto) get(Self&& self) {
    if constexpr (I == 0)
      return (self.left);
    else if constexpr (I == 1)
      return (self.right);
    else
      return (self.other);
  }
};

constexpr bool grouped_choice(const Choice& choice) {
  return choice match {
    case { Left || Right } => true;
    case _ => false;
  };
}

static_assert(grouped_choice({0, {}, {}, {}}));
static_assert(grouped_choice({1, {}, {}, {}}));
static_assert(!grouped_choice({2, {}, {}, {}}));

constexpr int use(const Left&) { return 1; }
constexpr int use(const Right&) { return 2; }
constexpr int use(int) { return 3; }
constexpr int use(long) { return 4; }

constexpr int differently_typed_binding(const Choice& choice) {
  return choice match {
    case { const Left& value } || { const Right& value } => use(value);
    case _ => 0;
  };
}

static_assert(differently_typed_binding({0, {}, {}, {}}) == 1);
static_assert(differently_typed_binding({1, {}, {}, {}}) == 2);
static_assert(differently_typed_binding({2, {}, {}, {}}) == 0);

constexpr int differently_typed_direct_binding(auto value) {
  return value match {
    case int bound || long bound => use(bound);
    case _ => 0;
  };
}

static_assert(differently_typed_direct_binding(1) == 3);
static_assert(differently_typed_direct_binding(1L) == 4);
static_assert(differently_typed_direct_binding(1.0) == 0);

constexpr int dependent(auto value) {
  return value match {
    case int || long => 1;
    case _ => 0;
  };
}

static_assert(dependent(1) == 1);
static_assert(dependent(1L) == 1);
static_assert(dependent(1.0) == 0);

struct Base { virtual ~Base(); };
struct Derived : Base {};
struct Sibling : Base {};

bool overlapping_initialization(Base& value) {
  return value match case Derived& || Sibling&;
}
