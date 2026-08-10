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
