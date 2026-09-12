// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

constexpr int select_two(int first, int second) {
  return match (first, second) {
    case [0, 0] => 1;
    case [0, int value] => value;
    case [int value, 0] => -value;
    case [int x, int y] => x + y;
  };
}

static_assert(select_two(0, 0) == 1);
static_assert(select_two(0, 3) == 3);
static_assert(select_two(4, 0) == -4);
static_assert(select_two(4, 5) == 9);

constexpr int select_booleans(bool first, bool second) {
  return match (first, second) {
    case [false, false] => 0;
    case [false, true] => 1;
    case [true, false] => 2;
    case [true, true] => 3;
  };
}

static_assert(select_booleans(false, false) == 0);
static_assert(select_booleans(false, true) == 1);
static_assert(select_booleans(true, false) == 2);
static_assert(select_booleans(true, true) == 3);

constexpr bool preserves_value_categories(int& lvalue, const int& constant,
                                          int&& xvalue) {
  return match (lvalue, constant, static_cast<int&&>(xvalue), 42) {
    case [auto&& a, auto&& b, auto&& c, auto&& d] =>
        __is_same(decltype(a), int&) &&
        __is_same(decltype(b), const int&) &&
        __is_same(decltype(c), int&&) &&
        __is_same(decltype(d), int&&);
  };
}

constexpr bool test_value_categories() {
  int lvalue = 1;
  const int constant = 2;
  int xvalue = 3;
  return preserves_value_categories(lvalue, constant,
                                    static_cast<int&&>(xvalue));
}

static_assert(test_value_categories());

constexpr bool structured_binding_declaration(int& first, int& second) {
  return match (first, second) {
    case auto&& [x, y] => __is_same(decltype(x), int&) &&
                         __is_same(decltype(y), int&) &&
                         &x == &first && &y == &second;
  };
}

constexpr bool test_structured_binding_declaration() {
  int first = 1;
  int second = 2;
  return structured_binding_declaration(first, second);
}

static_assert(test_structured_binding_declaration());

constexpr bool mutates_lvalue_subjects() {
  int first = 1;
  int second = 2;
  match (first, second) {
    case [int& x, int& y] => {
      x = 3;
      y = 4;
    }
  }
  return first == 3 && second == 4;
}

static_assert(mutates_lvalue_subjects());

struct MoveOnly {
  int value;

  constexpr explicit MoveOnly(int value) : value(value) {}
  MoveOnly(const MoveOnly&) = delete;
  constexpr MoveOnly(MoveOnly&& other) : value(other.value) {
    other.value = -1;
  }
};

constexpr int moves_rvalue_subjects() {
  return match (MoveOnly(3), MoveOnly(4)) {
    case [MoveOnly first, MoveOnly second] => first.value + second.value;
  };
}

static_assert(moves_rvalue_subjects() == 7);

struct Lifetime {
  int* alive;

  constexpr explicit Lifetime(int* alive) : alive(alive) { ++*alive; }
  Lifetime(const Lifetime&) = delete;
  constexpr ~Lifetime() { --*alive; }
};

constexpr bool extends_temporary_lifetimes() {
  int alive = 0;
  int observed = match (Lifetime(&alive), Lifetime(&alive)) {
    case [auto&&, auto&&] => alive;
  };
  return observed == 2 && alive == 0;
}

static_assert(extends_temporary_lifetimes());

template<class... Ts>
constexpr unsigned count_subjects(Ts&&... subjects) {
  return match (static_cast<Ts&&>(subjects)...) {
    case [auto&&... elements] => sizeof...(elements);
  };
}

static_assert(count_subjects() == 0);
static_assert(count_subjects(1) == 1);
static_assert(count_subjects(1, 2L, 3.0) == 3);

template<class... Ts>
constexpr unsigned count_tail(Ts&&... subjects) {
  return match (0, static_cast<Ts&&>(subjects)...) {
    case [0, auto&&... elements] => sizeof...(elements);
  };
}

static_assert(count_tail() == 0);
static_assert(count_tail(1, 2.0) == 2);

template<class First, class Second>
constexpr int select_constexpr(First&& first, Second&& second) {
  return match constexpr (static_cast<First&&>(first),
                          static_cast<Second&&>(second)) {
    case [int x, int y] => x + y;
    case [auto&&, auto&&] => static_assert(false);
  };
}

static_assert(select_constexpr(1, 2) == 3);

constexpr int comma_expression_remains_one_subject(int first, int second) {
  return match ((first, second)) {
    case int value => value;
  };
}

static_assert(comma_expression_remains_one_subject(1, 2) == 2);
