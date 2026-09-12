// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

template<class T, class U>
struct Pair {
  T first;
  U second;
};

constexpr int direct(int value) {
  return match (value) {
    case int => 1;
  };
}

constexpr Pair<int, long> pair{1, 2};

static_assert(direct(0) == 1);
static_assert(match(0, case int));
static_assert(match(pair, case [int, long]));
constexpr bool declaration_equivalent_types() {
  int value = 0;
  const int constant = 0;
  int array[2] = {};
  return (match(value, case int&)) && (match(value, case const int&)) &&
         (match(static_cast<int&&>(value), case int&&)) &&
         (match(constant, case int)) && (match(array, case const int*));
}

static_assert(declaration_equivalent_types());

void function_subject() noexcept;
using Function = void();
static_assert(match(function_subject, case Function*));

struct CopyCounter {
  int* copies;

  constexpr explicit CopyCounter(int& copies) : copies(&copies) {}
  constexpr CopyCounter(const CopyCounter& other) : copies(other.copies) {
    ++*copies;
  }
};

constexpr bool unnamed_value_pattern_does_not_initialize() {
  int copies = 0;
  CopyCounter value(copies);
  bool matched = match(value, case CopyCounter);
  return matched && copies == 0;
}

constexpr bool unnamed_reference_pattern_does_not_copy() {
  int copies = 0;
  CopyCounter value(copies);
  bool matched = match(value, case CopyCounter&);
  return matched && copies == 0;
}

constexpr bool nested_unnamed_value_pattern_does_not_initialize() {
  int copies = 0;
  Pair<CopyCounter, int> value{CopyCounter(copies), 0};
  bool matched = match(value, case [CopyCounter, int&]);
  return matched && copies == 0;
}

struct MoveCounter {
  int* moves;

  constexpr explicit MoveCounter(int& moves) : moves(&moves) {}
  MoveCounter(const MoveCounter&) = delete;
  constexpr MoveCounter(MoveCounter&& other) : moves(other.moves) { ++*moves; }
};

constexpr bool unnamed_value_pattern_does_not_move_from_xvalue() {
  int moves = 0;
  MoveCounter value(moves);
  bool matched = match(static_cast<MoveCounter&&>(value), case MoveCounter);
  return matched && moves == 0;
}

constexpr int repeated_type_patterns_do_not_initialize() {
  int copies = 0;
  CopyCounter value(copies);
  int result = match (value) {
    case CopyCounter if (false) => 0;
    case CopyCounter => copies;
  };
  return result * 10 + copies;
}

static_assert(unnamed_value_pattern_does_not_initialize());
static_assert(unnamed_reference_pattern_does_not_copy());
static_assert(nested_unnamed_value_pattern_does_not_initialize());
static_assert(unnamed_value_pattern_does_not_move_from_xvalue());
static_assert(repeated_type_patterns_do_not_initialize() == 0);

struct Constructed {
  friend constexpr bool operator==(Constructed, Constructed) = default;
};

static_assert(match(Constructed{}, case static_cast<Constructed>(Constructed{})));
static_assert(match(Constructed{}, case (Constructed)));

constexpr void increment(int& value) {
  ++value;
}

constexpr int direct_void() {
  int evaluations = 0;
  int result = match (increment(evaluations)) {
    case const void => evaluations;
  };
  return result * 10 + evaluations;
}

static_assert(direct_void() == 11);

constexpr bool direct_void_test() {
  int evaluations = 0;
  bool result = match(increment(evaluations), case const volatile void);
  return result && evaluations == 1;
}

static_assert(direct_void_test());

struct VoidResult {
  constexpr void f() const {}
};

struct IntResult {
  constexpr int f() const { return 4; }
};

template<class T>
constexpr int dependent_result_type(T value) {
  return match (value.f()) {
    case void => 1;
    case int result => result;
  };
}

template<class T>
constexpr bool has_void_result(T value) {
  return requires(T candidate) { match(candidate.f(), case void); };
}

static_assert(dependent_result_type(VoidResult{}) == 1);
static_assert(dependent_result_type(IntResult{}) == 4);
static_assert(has_void_result(VoidResult{}));
static_assert(!has_void_result(IntResult{}));

struct Left {
  int left;
};

struct Right {
  int right;
};

template<class T>
constexpr int dependent(T value) {
  return match (value) {
    case Left => value.left;
    case Right => value.right;
  };
}

static_assert(dependent(Left{3}) == 3);
static_assert(dependent(Right{4}) == 4);

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

struct VoidOrInt {
  bool has_value;
  int value;
};

template<>
struct std::alternative_traits<VoidOrInt> {
  static constexpr alternative_info alternatives[] = {
    ^^void, ^^int
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const VoidOrInt& value) noexcept {
    return value.has_value ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& value) {
    if constexpr (I == 0)
      return;
    else
      return (static_cast<Self&&>(value).value);
  }
};

constexpr int project_void(VoidOrInt value) {
  return match (value) {
    case { void } => 5;
    case { int } => 6;
  };
}

static_assert(project_void({true, 0}) == 5);
static_assert(project_void({false, 0}) == 6);
