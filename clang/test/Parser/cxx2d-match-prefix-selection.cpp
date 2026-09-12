// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -Wno-unused-value -verify %s

// This test collects the syntactic classification and diagnostic recovery for
// prefix match selections. In particular, `match` remains an identifier, and
// `->` can begin either an explicit selection result type or member access.

namespace selections {
constexpr int expression(int value) {
  return match (value) {
    case 0 => 1;
    case _ => 2;
  };
}

void statement(int value) {
  match (value) {
    case 0 => 1;
    case _ => "other";
  }

  if (value)
    match (value) { case _ => 0; }
  else
    match (value) { case _ => 1; }
}

constexpr int multiple_subjects(int first, int second) {
  return match (first, second) {
    case [0, int value] => value;
    case [int value, 0] => value;
    case _ => -1;
  };
}

constexpr int comma_expression_is_one_subject(int first, int second) {
  return match ((first, second)) {
    case int value => value;
  };
}

static_assert(multiple_subjects(0, 42) == 42);
static_assert(comma_expression_is_one_subject(1, 2) == 2);

void missing_subject() {
  match () { // expected-error {{expected expression}}
    case [] =>;
  }
}

void invalid_multiple_subject(int value) {
  match (value, static_cast<void>(value)) { // expected-error {{void expression cannot be an element of a multiple-subject match}}
    case _ =>;
  }
}

void consume(int);

void statement_handlers(int value) {
  match (value) {
    case 0 => {
      int local = value;
      consume(local);
    }
    case 1 => if (value) consume(value); else consume(0);
    case 2 => for (int i = 0; i != value; ++i) consume(i);
    case 3 => while (value-- > 0) consume(value);
    case 4 => do consume(value); while (false);
    case 5 => switch (value) { default: break; }
    case 7 => int local = value;
    case 8 => static_assert(true);
    case _ => ;
  }
}

void missing_case(int value) {
  match (value) {
    _ => 0; // expected-error {{expected 'case' before pattern}}
  }
}

struct Incomplete;

Incomplete* incomplete_result(int value) {
  return match (value) -> Incomplete* {
    case _ => nullptr;
  };
}

constexpr int explicit_result() {
  return match constexpr (0) -> int {
    case 0 => 1;
    case _ => 2;
  };
}

int may_return();

int nonreturning_handler(int value) {
  return match (value) {
    case 0 => not return may_return();
    case _ => 42;
  };
}

constexpr int noreturn = 7;

constexpr int noreturn_is_an_ordinary_expression(int value) {
  return match (value) {
    case 0 => noreturn + 1;
    case _ => 42;
  };
}

namespace ordinary_noreturn_call {
constexpr int noreturn(int value) { return value + 2; }

constexpr int call(int value) {
  return match (value) {
    case 0 => noreturn(value);
    case _ => 42;
  };
}
} // namespace ordinary_noreturn_call

int symbolic_nonreturning_handler(int value) {
  return match (value) {
    case 0 => !return may_return();
    case _ => 42;
  };
}

void noreturn_is_ordinary_in_statement_form(int value) {
  match (value) {
    case 0 => noreturn;
    case _ => ;
  }
}

static_assert(expression(0) == 1);
static_assert(expression(3) == 2);
static_assert(explicit_result() == 1);
static_assert(noreturn_is_an_ordinary_expression(0) == 8);
static_assert(ordinary_noreturn_call::call(0) == 2);
} // namespace selections

namespace missing_subject_parentheses {
void statement(int value) {
  match value { // expected-error {{expected '(' after 'match'}}
    case _ =>;
  }
}

int expression(int value) {
  return match value { // expected-error {{expected '(' after 'match'}}
    case _ => 0;
  };
}

int compound_subject(int value) {
  return match value + 1 { // expected-error {{expected '(' after 'match'}}
    case _ => 0;
  };
}

int no_ordinary_name() {
  return match + 1; // expected-error {{expected '(' after 'match'}}
}
} // namespace missing_subject_parentheses

namespace declaration_disambiguation {
namespace constants {
inline constexpr int one = 1;
}

struct match {
  match() = default;
  match(int);

  template<class F>
  match(F);
};

void declarations_and_selections(int value) {
  match object(0);
  match default_initialized;
  match (function)(); // expected-warning {{empty parentheses interpreted as a function declaration}} expected-note {{remove parentheses to declare a variable}}
  match (braced) { 1 };
  match (lambda) { [] { return 1; }() };
  match (capturing_lambda) { [value] { return value; }() };
  match (*pointer) {};
  match (array[2]) { 1, 2 };

  match (value) { case _ => 0; }
  match (value) { case _ => 0; }
  match (value) { [[likely]] case _ => 0; }
  match (value + 1) { case _ => 0; }

  match (value) {
    using Alias = int;
    namespace c = constants;
    static_assert(sizeof(Alias) == sizeof(int));
    case c::one => 1;
    case Alias copy => copy;
  }
}

match expression_construction() {
  return match{0};
}

void missing_case(int value) {
  match (value) { // expected-note {{'match' names a type, so this construct is otherwise parsed as a declaration}}
    _ => 0; // expected-error {{expected 'case' before pattern}}
  }
}
} // namespace declaration_disambiguation

namespace ordinary_expressions {
int match(int);

int call() { return match(1); }
int arithmetic() { return match(1) + 1; }

struct CallableResult {
  int operator()() const;
};

CallableResult match(long);
int chained_call() { return match(1L)(); }

struct Node {
  int value;
  CallableResult callable;
};

Node* match(short);

int member_access() { return match(short{0})->value; }
int member_call() { return match(short{0})->callable(); }
int member_plus_braced_operand() { return match(short{0})->value + int{1}; }
int member_plus_lambda() {
  return match(short{0})->value + [] {
    [[maybe_unused]] int local = 1;
    return 1;
  }();
}

void selection_with_function_in_scope(int value) {
  match (value) { case _ => 0; }
}

struct result {};

result selection_overrides_ordinary_call(int value) {
  return match (value) -> result {
    case _ => result{};
  };
}

void expression_statements() {
  match(short{0})->value;
  match(short{0})->callable();
}
} // namespace ordinary_expressions

namespace non_type_match_names {
void local_variable(int value) {
  int match = 0;
  int result = match + 1;
  match (value) { case _ => match; }
}

struct Callable {
  int operator()(int) const;
};

void callable_object(int value) {
  Callable match;
  int result = match(value);
  match (value) { case _ => result; }
}

void label() {
match:
  return;
}
} // namespace non_type_match_names

namespace statement_result_recovery {
void result_types_are_not_allowed(int value) {
  match (value) -> int { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (value) -> const MissingResult& { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (value) -> int; // expected-error {{match statement cannot have an explicit result type}}
}
} // namespace statement_result_recovery

namespace expression_result_recovery {
struct Result {};

int missing_body(int value) {
  return match(value)->Result; // expected-error {{expected '{' after match result type}}
}

int unknown_result(int value) {
  return match(value)->MissingResult; // expected-error {{unknown type name 'MissingResult'}}
}

template<class T>
struct ResultVector {}; // expected-note 2 {{'ResultVector' declared here}}

ResultVector<int> corrected_result(int value) {
  return match(value) -> ResltVector<int> { // expected-error {{no template named 'ResltVector'; did you mean 'ResultVector'?}}
    case _ => ResultVector<int>{};
  };
}

ResultVector<int> brace_alone_establishes_match(int value) {
  return match(value) -> ResltVector<int> { // expected-error {{no template named 'ResltVector'; did you mean 'ResultVector'?}}
    not_an_arm // expected-error {{expected 'case' before pattern}}
  };
}

struct Node {
  int member;
};

Node* match(int); // expected-note 2 {{candidate function not viable}}

int ordinary_member_diagnostic(int value) {
  return match(value)->missing; // expected-error {{no member named 'missing' in 'expression_result_recovery::Node'}}
}

int ordinary_overload_diagnostic() {
  return match("wrong")->member; // expected-error {{no matching function for call to 'match'}}
}

namespace associated {
struct Key {};
Node* match(Key) = delete; // expected-note {{candidate function has been explicitly deleted}}
}

int ordinary_deleted_adl_diagnostic(associated::Key key) {
  return match(key)->member; // expected-error {{call to deleted function 'match'}}
}

int invalid_subject() {
  return match(unknown_subject)->Result; // expected-error {{use of undeclared identifier 'unknown_subject'}}
}
} // namespace expression_result_recovery

namespace candidate_kinds {
struct Node {
  int member;
};

struct match {
  match(int);
  Node* operator->();
};

int type_name(int value) {
  return match(value)->member;
}

namespace callable_object {
struct Callable {
  Node* operator()(int) const;
} match;

int call(int value) {
  return match(value)->member;
}
} // namespace callable_object
} // namespace candidate_kinds

namespace multi_argument_adl {
struct Node {
  int member;
};

namespace associated {
struct Key {};
Node* match(Key, int);
} // namespace associated

int every_argument_participates(associated::Key key) {
  return match(key, 0)->member;
}
} // namespace multi_argument_adl

namespace dependent_recovery {
struct Result {
  int member;
};

namespace valid_adl {
struct Key {};
Result* match(Key);
}

template<class T>
int dependent_call(T value) {
  return match(value)->member;
}

int instantiate_valid_call() {
  return dependent_call(valid_adl::Key{});
}

template<class T>
int missing_candidate(T value) {
  return match(value)->Result; // expected-error {{expected '{' after match result type}}
}

int instantiate_missing_candidate() {
  return missing_candidate(0); // expected-note {{in instantiation of function template specialization}}
}

namespace nonviable_adl {
struct Key {};
Result* match(Key, int); // expected-note {{candidate function not viable: requires 2 arguments, but 1 was provided}}
}

template<class T>
int nonviable_candidate(T value) {
  return match(value)->member; // expected-error {{no matching function for call to 'match'}}
}

int instantiate_nonviable_candidate() {
  return nonviable_candidate(nonviable_adl::Key{}); // expected-note {{in instantiation of function template specialization}}
}

template<class T>
int invalid_nested_argument(T value) {
  return match(other(value))->Result; // expected-error {{use of undeclared identifier 'other'}}
}

int instantiate_invalid_nested_argument() {
  return invalid_nested_argument(0); // expected-note {{in instantiation of function template specialization}}
}

template<class T, class U>
int explicit_template_arguments_are_ordinary(T value) {
  return match<U>(value)->Result; // expected-error {{use of undeclared identifier 'match'}}
}

int instantiate_explicit_template_arguments() {
  return explicit_template_arguments_are_ordinary<int, int>(0); // expected-note {{in instantiation of function template specialization}}
}
} // namespace dependent_recovery
