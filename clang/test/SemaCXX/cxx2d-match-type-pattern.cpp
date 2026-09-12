// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

int redundant_type(int value) {
  return match (value) {
    case double => 0; // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
    case _ => 1;
  };
}

int impossible_guarded_type(int value) {
  return match (value) {
    case double if (value > 0) => 0; // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
    case _ => 1;
  };
}

bool incompatible_single_type_pattern(int value) {
  return match(value, case double); // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
}

constexpr const int constant = 0;
static_assert(match(constant, case int));
static_assert(match(constant, case const int));
static_assert(match(constant, case auto));
static_assert(match(constant, case auto&&));

template<class T>
concept Integral = __is_integral(T);

template<class T>
concept LvalueReference = __is_lvalue_reference(T);

constexpr int unnamed_constrained_pattern(auto&& value) {
  return match (static_cast<decltype(value)>(value)) {
    case Integral auto => 1;
    case _ => 0;
  };
}

static_assert(unnamed_constrained_pattern(0) == 1);
static_assert(unnamed_constrained_pattern(0.0) == 0);

constexpr int unnamed_constrained_forwarding_pattern(auto&& value) {
  return match (static_cast<decltype(value)>(value)) {
    case LvalueReference auto&& => 1;
    case _ => 0;
  };
}

static_assert([] {
  int value = 0;
  return unnamed_constrained_forwarding_pattern(value);
}() == 1);
static_assert(unnamed_constrained_forwarding_pattern(0) == 0);

void invalid_unnamed_decl_specifiers(int value) {
  // expected-error@+1 {{type name does not allow storage class to be specified}}
  bool storage = match(value, case static int);
  // expected-error@+1 {{type name does not allow constexpr specifier to be specified}}
  bool constant = match(value, case constexpr int);
  // expected-error@+1 {{type name does not allow storage class to be specified}}
  bool type_alias = match(value, case typedef int);
}

struct Pair {
  int first;
  long second;
};

static_assert(match(Pair{1, 2}, case [int, long]));
bool incompatible_nested_type_pattern(Pair value) {
  return match(value, case [long, int]); // expected-error {{type pattern of type 'long' is not an exact match for subject of type 'int'}}
}

struct NoMatch {};

int redundant_nested(Pair value) {
  return match (value) {
    case [int, NoMatch] => 0; // expected-error {{type pattern of type 'NoMatch' is not an exact match for subject of type 'long'}}
    case _ => 1;
  };
}

struct NonCopyable {
  NonCopyable();
  NonCopyable(const NonCopyable&) = delete; // expected-note {{has been explicitly marked deleted here}}
};

bool invalid_hypothetical_initialization(NonCopyable& value) {
  return match(value, case NonCopyable); // expected-error {{call to deleted constructor of 'NonCopyable'}}
}

template<class U>
constexpr int dependent_type_pattern(int value) {
  return match (value) {
    case U => 1;
    case _ => 0;
  };
}

static_assert(dependent_type_pattern<int>(0) == 1);
static_assert(dependent_type_pattern<double>(0) == 0);

template<class T>
concept MatchesIntTypePattern = requires(T value) {
  match(value, case int);
};

static_assert(MatchesIntTypePattern<int>);
static_assert(!MatchesIntTypePattern<double>);

template<class T>
bool dependent_single_type_pattern(T value) {
  return match(value, case int); // expected-error {{type pattern of type 'int' is not an exact match for subject of type 'double'}}
}

bool instantiate_invalid_single_type_pattern() {
  return dependent_single_type_pattern(0.0); // expected-note {{in instantiation of function template specialization 'dependent_single_type_pattern<double>' requested here}}
}
