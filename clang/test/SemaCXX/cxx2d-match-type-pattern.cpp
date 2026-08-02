// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

int redundant_type(int value) {
  return value match {
    case double => 0; // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
    case _ => 1;
  };
}

int impossible_guarded_type(int value) {
  return value match {
    case double if (value > 0) => 0; // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
    case _ => 1;
  };
}

bool incompatible_single_type_pattern(int value) {
  return value match case double; // expected-error {{type pattern of type 'double' is not an exact match for subject of type 'int'}}
}

constexpr const int constant = 0;
static_assert(constant match case int);
static_assert(constant match case const int);

struct Pair {
  int first;
  long second;
};

static_assert(Pair{1, 2} match case [int, long]);
bool incompatible_nested_type_pattern(Pair value) {
  return value match case [long, int]; // expected-error {{type pattern of type 'long' is not an exact match for subject of type 'int'}}
}

struct NoMatch {};

int redundant_nested(Pair value) {
  return value match {
    case [int, NoMatch] => 0; // expected-error {{type pattern of type 'NoMatch' is not an exact match for subject of type 'long'}}
    case _ => 1;
  };
}

struct NonCopyable {
  NonCopyable();
  NonCopyable(const NonCopyable&) = delete; // expected-note {{has been explicitly marked deleted here}}
};

bool invalid_hypothetical_initialization(NonCopyable& value) {
  return value match case NonCopyable; // expected-error {{call to deleted constructor of 'NonCopyable'}}
}

template<class U>
constexpr int dependent_type_pattern(int value) {
  return value match {
    case U => 1;
    case _ => 0;
  };
}

static_assert(dependent_type_pattern<int>(0) == 1);
static_assert(dependent_type_pattern<double>(0) == 0);

template<class T>
concept MatchesIntTypePattern = requires(T value) {
  value match case int;
};

static_assert(MatchesIntTypePattern<int>);
static_assert(!MatchesIntTypePattern<double>);

template<class T>
bool dependent_single_type_pattern(T value) {
  return value match case int; // expected-error {{type pattern of type 'int' is not an exact match for subject of type 'double'}}
}

bool instantiate_invalid_single_type_pattern() {
  return dependent_single_type_pattern(0.0); // expected-note {{in instantiation of function template specialization 'dependent_single_type_pattern<double>' requested here}}
}
