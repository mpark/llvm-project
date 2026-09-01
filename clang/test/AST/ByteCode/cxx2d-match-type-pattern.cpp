// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

template<class T, class U>
struct Pair {
  T first;
  U second;
};

constexpr int direct(int value) {
  return value match {
    case int => 1;
  };
}

constexpr Pair<int, long> pair{1, 2};

static_assert(direct(0) == 1);
static_assert(0 match case int);
static_assert(pair match case [int, long]);
constexpr bool declaration_equivalent_types() {
  int value = 0;
  const int constant = 0;
  int array[2] = {};
  return (value match case int&) && (value match case const int&) &&
         (static_cast<int&&>(value) match case int&&) &&
         (constant match case int) && (array match case const int*);
}

static_assert(declaration_equivalent_types());

void function_subject() noexcept;
static_assert(function_subject match case void (*)());

struct CopyCounter {
  int* copies;

  constexpr explicit CopyCounter(int& copies) : copies(&copies) {}
  constexpr CopyCounter(const CopyCounter& other) : copies(other.copies) {
    ++*copies;
  }
};

constexpr bool unnamed_value_pattern_initializes() {
  int copies = 0;
  CopyCounter value(copies);
  bool matched = value match case CopyCounter;
  return matched && copies == 1;
}

constexpr bool unnamed_reference_pattern_does_not_copy() {
  int copies = 0;
  CopyCounter value(copies);
  bool matched = value match case CopyCounter&;
  return matched && copies == 0;
}

constexpr bool nested_unnamed_value_pattern_initializes() {
  int copies = 0;
  Pair<CopyCounter, int> value{CopyCounter(copies), 0};
  bool matched = value match case [CopyCounter, int&];
  return matched && copies == 1;
}

struct MoveCounter {
  int* moves;

  constexpr explicit MoveCounter(int& moves) : moves(&moves) {}
  MoveCounter(const MoveCounter&) = delete;
  constexpr MoveCounter(MoveCounter&& other) : moves(other.moves) { ++*moves; }
};

constexpr bool unnamed_value_pattern_moves_from_xvalue() {
  int moves = 0;
  MoveCounter value(moves);
  bool matched = static_cast<MoveCounter&&>(value) match case MoveCounter;
  return matched && moves == 1;
}

constexpr int each_selected_arm_initializes() {
  int copies = 0;
  CopyCounter value(copies);
  int result = value match {
    case CopyCounter if (false) => 0;
    case CopyCounter => copies;
  };
  return result * 10 + copies;
}

static_assert(unnamed_value_pattern_initializes());
static_assert(unnamed_reference_pattern_does_not_copy());
static_assert(nested_unnamed_value_pattern_initializes());
static_assert(unnamed_value_pattern_moves_from_xvalue());
static_assert(each_selected_arm_initializes() == 22);

struct Constructed {
  friend constexpr bool operator==(Constructed, Constructed) = default;
};

static_assert(Constructed{} match case (Constructed()));

constexpr void increment(int& value) {
  ++value;
}

constexpr int direct_void() {
  int evaluations = 0;
  int result = increment(evaluations) match {
    case const void => evaluations;
  };
  return result * 10 + evaluations;
}

static_assert(direct_void() == 11);

constexpr bool direct_void_test() {
  int evaluations = 0;
  bool result = increment(evaluations) match case const volatile void;
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
  return value.f() match {
    case void => 1;
    case int result => result;
  };
}

template<class T>
constexpr bool has_void_result(T value) {
  return requires(T candidate) { candidate.f() match case void; };
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
  return value match {
    case Left => value.left;
    case Right => value.right;
  };
}

static_assert(dependent(Left{3}) == 3);
static_assert(dependent(Right{4}) == 4);

namespace std {
template<class T>
struct alternative_traits;
}

struct VoidOrInt {
  bool has_value;
  int value;
};

template<>
struct std::alternative_traits<VoidOrInt> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
  using type = __type_pack_element<I, void, int>;

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
  return value match {
    case { void } => 5;
    case { int } => 6;
  };
}

static_assert(project_void({true, 0}) == 5);
static_assert(project_void({false, 0}) == 6);
