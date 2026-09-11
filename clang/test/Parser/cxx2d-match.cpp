// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -Wno-unused-variable -Wno-unused-value -verify %s

inline constexpr int empty_state = 0;

namespace std {
template <class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};

template <class T>
struct alternative_traits<T *> {
  static constexpr alternative_info alternatives[] = {
    {^^empty_state, true}, ^^T
  };
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(T *pointer) noexcept {
    return pointer ? 1 : 0;
  }

  template <__SIZE_TYPE__ I, class Self>
    requires(I == 1)
  static constexpr decltype(auto) get(Self &&self) {
    return *self;
  }
};
} // namespace std

void test_match_is_not_keyword() {
  int match;
  int foo(int match);
  {
    struct foo {};
    struct match {};
    {
      {
        match foo;
        match match;
      }
      foo match;
    }
    {
      {
        match foo{};
        match match{};
      }
      foo match{};
    }
    {
      {
        match foo = {};
        match match = {};
      }
      foo match = {};
    }
  }
  {
    using B = bool;
    bool match = true;
    !(B)match;
  }
}

void test_let_is_not_keyword() {
  int let;
  int foo(int let);
  {
    struct let {};
    let let;
  }
}

void test_match_no_rhs(int i) {
  42 match; // expected-error {{expected case}}
  42 match constexpr; // expected-error {{expected case}}
  42 match -> ; // expected-error {{expected case}}
  42 match->i; // expected-error {{expected case}}
  42 match -> void; // expected-error {{expected case}}
}

void test_case_is_required(int x) {
  x match _; // expected-error {{expected case}}
  x match { _ => 0; case _ => 0; }; // expected-error {{expected case}}
}

void test_match_structures(int x) {
  x match case _;
  &x match case { _ };
  x match case 0;
  match (x) { case _ => 0; }
  match (x) { case _ if (true) => 0; case _ => 0; }
  match constexpr (x) { case _ => 0; }
  match constexpr (x) { case _ if (true) => 0; case _ => 0; }
  (void)match (x) -> int { case _ => 0; };
  (void)match (x) -> auto { case _ => 0; };
  (void)match (x) -> decltype(auto) { case _ => 0; };
  (void)match (x) -> int { case _ if (true) => 0; case _ => 0; };
  (void)match (x) -> auto { case _ if (true) => 0; case _ => 0; };
  (void)match (x) -> decltype(auto) { case _ if (true) => 0; case _ => 0; };
  (void)match constexpr (x) -> int { case _ => 0; };
  (void)match constexpr (x) -> auto { case _ => 0; };
  (void)match constexpr (x) -> decltype(auto) { case _ => 0; };
  (void)match constexpr (x) -> int { case _ if (true) => 0; case _ => 0; };
  (void)match constexpr (x) -> auto { case _ if (true) => 0; case _ => 0; };
  (void)match constexpr (x) -> decltype(auto) { case _ if (true) => 0; case _ => 0; };
  match (&x) { case { _ } => 0; case _ => 1; }
}

void test_match_statement(int x) {
  match (x) {
    case 0 => 0;
    case _ => "zero";
  }

  if (x)
    match (x) { case _ => 0; }
  else
    match (x) { case _ => 1; }
}

void test_match_statement_result_type(int x) {
  match (x) -> int { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (x) -> const MissingStatementResult& { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (x) -> MissingStatementResult { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (x) -> typename missing_statement_result::type { // expected-error {{match statement cannot have an explicit result type}}
    case _ => 0;
  }

  match (x) -> int; // expected-error {{match statement cannot have an explicit result type}}
}

void test_missing_case_diagnostics(int x) {
  match (x) {
    _ => 0; // expected-error {{expected 'case' before pattern}}
  }

  match (x) {
    [[likely]] _ => 0; // expected-error {{expected 'case' before pattern}}
  }

  match (x) {
    [[likely] case _ => 0; // expected-error {{expected ']'}}
  } // expected-error {{expected 'case' before pattern}}
}

constexpr int test_prefix_match_expression(int x) {
  return match (x) {
    case 0 => 1;
    case _ => 2;
  };
}

struct IncompleteResult;

IncompleteResult* test_incomplete_result_type(int x) {
  return match (x) -> IncompleteResult* {
    case _ => nullptr;
  };
}

int test_unknown_result_type(int x) {
  return match (x) -> UnknownResult { // expected-error {{unknown type name 'UnknownResult'}}
    case _ => 0;
  };
}

int test_unknown_cvref_result_type(int x) {
  return match (x) -> const UnknownResult& { // expected-error {{unknown type name 'UnknownResult'}}
    case _ => 0;
  };
}

int test_unknown_qualified_result_type(int x) {
  return match (x) -> typename missing_result::type { // expected-error {{use of undeclared identifier 'missing_result'}}
    case _ => 0;
  };
}

template<class T>
struct OneArgument; // expected-note {{template is declared here}}

int test_invalid_template_result_type(int x) {
  return match (x) -> OneArgument<int, long> { // expected-error {{too many template arguments for class template 'OneArgument'}}
    case _ => 0;
  };
}

int test_invalid_decltype_result_type(int x) {
  return match (x) -> decltype(missing_result) { // expected-error {{use of undeclared identifier 'missing_result'}}
    case _ => 0;
  };
}

int test_missing_match_body_after_result_type(int x) {
  return match (x) -> int; // expected-error {{expected '{' after match result type}}
}

int test_invalid_unambiguous_result_type_without_body(int x) {
  return match (x) -> const MissingUnambiguousResult&; // expected-error {{unknown type name 'MissingUnambiguousResult'}}
}

int test_invalid_decltype_result_without_body(int x) {
  return match (x) -> decltype(missing_decltype_operand); // expected-error {{use of undeclared identifier 'missing_decltype_operand'}}
}

static_assert(test_prefix_match_expression(0) == 1);
static_assert(test_prefix_match_expression(3) == 2);
static_assert((match (0) { case 0 => true; case _ => false; }));

namespace prefix_match_disambiguation {
struct match {
  match(int);

  template<class F>
  match(F);
};

void declarations_and_statements(int x) {
  match object(0);
  match (function)(); // expected-warning {{empty parentheses interpreted as a function declaration}} expected-note {{remove parentheses to declare a variable}}
  match (braced) { 1 };
  match (lambda) { [] { return 1; }() };
  match (capturing_lambda) { [x] { return x; }() };
  match (*pointer) {};
  match (array[2]) { 1, 2 };
  match (x) { case _ => 0; }
  match (*pointer) { case _ => 0; }
  match (x) { case _ => 0; }
  match (x + 1) { case _ => 0; }
  match (x) { [[likely]] case _ => 0; }
}

int expression(int x) {
  return match (x) { case _ => x; };
}
} // namespace prefix_match_disambiguation

namespace prefix_match_call_disambiguation {
int match(int);

int call() { return match(1); }
int call_arithmetic() { return match(1) + 1; }

struct CallableResult {
  int operator()() const;
};

CallableResult match(long);

int chained_call() { return match(1L)(); }

struct MemberResult {
  int value;
  CallableResult callable;
};

struct value {};

MemberResult* match(short);

int member_access() { return match(short{0})->value; }
int member_call() { return match(short{0})->callable(); }
int member_plus_braced_operand() { return match(short{0})->value + int{1}; }
int member_comma_braced_operand() { return (match(short{0})->value, int{1}); }
int member_plus_attributed_lambda() {
  return match(short{0})->value + [] {
    [[maybe_unused]] int local = 1;
    return 1;
  }();
}

value member_name_is_result_type(int x) {
  return match (x) -> value {
    case 0 => value{};
    case _ => value{};
  };
}

void member_access_statement() {
  match(short{0})->value;
  match(short{0})->callable();
}

void statement(int x) {
  match (x) { case _ => 0; }
}

struct Callable {
  int operator()(int) const;
};

void callable(int x) {
  Callable match;
  int result = match(x);
  match (x) { case _ => result; }
}
} // namespace prefix_match_call_disambiguation

namespace prefix_match_hidden_type_disambiguation {
struct match {};

void statement(int x) {
  int match = 0;
  match (x) { case _ => 0; }
}
} // namespace prefix_match_hidden_type_disambiguation

namespace prefix_match_missing_case_disambiguation {
struct match {
  match(int);

  template<class T>
  match(T);
};

struct Pair {
  int first;
  int second;
};

void statement(int x) {
  match (x) { // expected-note {{'match' names a type, so this construct is otherwise parsed as a declaration}}
    _ => 0; // expected-error {{expected 'case' before pattern}}
  }

  match (x) { // expected-note {{'match' names a type, so this construct is otherwise parsed as a declaration}}
    int value => value; // expected-error {{expected 'case' before pattern}}
  }

  Pair pair{};
  match (pair) { // expected-note {{'match' names a type, so this construct is otherwise parsed as a declaration}}
    [0, int second] => second; // expected-error {{expected 'case' before pattern}}
  }

  match (x) { // expected-note {{'match' names a type, so this construct is otherwise parsed as a declaration}}
    int value if (int adjusted = value + 1; adjusted > 0) => adjusted; // expected-error {{expected 'case' before pattern}}
  }
}
} // namespace prefix_match_missing_case_disambiguation

struct PatternPair {
  int first;
  int second;
};

int test_declaration_pattern_before_comma(PatternPair pair) {
  return match (pair) {
    case [0, 0] => 0;
    case [0, int y] => y;
    case [int x, 0] => x;
    case auto [x, y] => x + y;
  };
}

void test_match_precedence(int* p) {
  /* MatchTestExpr */ {
    // unary is tighter than match
    *p match case 0;
    *p match case 0 + 1;
    // match binds tighter than bin ops.
    4 + 2 match case 0;
    4 * 2 match case 0;
    true == 2 match case 0;
    4 * (2) match case 0;
    2 match case 0 + 1;
    2 match case 0 * 1;
    2 match case 0 == 1;
    (2) match case 0 * 1;
    // except .* and ->*
    struct S { int i; } s;
    s.*&S::i match case 0;
    &s->*&S::i match case 0;
    2 match case s.*&S::i;
    2 match case &s->*&S::i;
    // unary parenthesized
    !(p match case nullptr);
    !((p) match case nullptr);
  }
  /* MatchSelectExpr */ {
    // unary is tighter than match
    match (*p) { case _ => 0; }
    (match (*p) { case _ => 0; }) + 1;
    // match binds tighter than bin ops.
    4 + match (2) { case _ => 0; };
    4 * match (2) { case _ => 0; };
    4 == match (2) { case _ => 0; };
    4 * match ((2)) { case _ => 0; };
    (match (2) { case _ => 0; }) + 1;
    (match (2) { case _ => 0; }) * 1;
    (match (2) { case _ => 0; }) == 1;
    (match ((2)) { case _ => 0; }) * 1;
    // except .* and ->*
    struct S { int i; } s;
    match (s.*&S::i) { case _ => 0; }
    match (&s->*&S::i) { case _ => 0; }
    (match (2) { case _ => s; }) .* &S::i;
    (match (2) { case _ => &s; }) ->* &S::i;
    // unary parenthesized
    !(match (p) { case _ => 0; });
    !(match ((p)) { case _ => 0; });
  }
}

void test_wildcard_pattern(int x) {
  x match case _;
  bool b = x match case _;
  match (x) { case _ => 0; }
}

void test_expression_pattern(int x, int y) {
  x match case 0;
  x match case (1 + 2);
  x match case y;
  int _ = 0;
  x match case +_;
  x match case -_;
  x match case y + 1;
  x match case auto(_ + 1);
  match (x) {
    case y + 1 => 0;
    case auto(_ + 1) => 0;
    case auto([] { return 2; }()) => 0;
    case auto([]<class T>(T value) { return value; }(3)) => 0;
    case _ => 0;
  }
  x match case auto((int)y);
  using Int = int;
  x match case auto((Int)y);
  x match case auto((Int)(y));
  x match case auto(((Int)(y)));
  constexpr auto id = [](auto &&x) -> auto && {
    return static_cast<decltype(x)>(x);
  };
  {
    int let = 42;
    x match case id(let);
    match (x) { case id(let) => 0; case _ => 0; }
  }
  {
    constexpr int let[2] = {1, 2};
    constexpr int idx = 0;
    match (x) { case id(let[idx]) => 0; case _ => 0; }
  }
  match (x) {
    case y++ => 0;
    case y++ * 2 => 0;
    case (y++) => 0;
    case auto((y)++ * 2) => 0;
    case _ => 0;
  }
}

void test_declaration_pattern(int i) {
  i match case auto&& x;
  x; // expected-error {{use of undeclared identifier 'x'}}
  match (i) { case auto&& x => 0; }
  match (i) { case auto&& x => x; }
  match (i) { case auto&& [x] => 0; case _ => 0; } // expected-error {{cannot bind non-class, non-array type 'int'}}
  int i1[1] = {0};
  match (i1) { case auto&& [x] => 0; }
  match (i1) { case auto&& [x] => x; }
  int i2[2] = {0, 0};
  match (i2) { case auto&& [x, y] => 0; }
  match (i2) { case auto&& [x, y] => x + y; }
  match (i2) { case [auto&& x, auto&& y] => 0; }
  match (i2) { case [auto&& x, auto&& y] =>  x + y; }
}

template <class T>
concept Integral = __is_integral(T);

void test_constrained_declaration_pattern(int i) {
  i match case Integral auto value;
  match (i) { case Integral auto value => value; }
  match (i) { case Integral auto => 0; }
  match (i) { case Integral auto Integral => Integral; }
}

constexpr int declaration_pattern_constant = 1;

void test_declaration_expression_disambiguation(int value) {
  match (value) { case int(declaration_pattern_constant) => 0; case _ => 0; }
  match (value) { case auto(declaration_pattern_constant) => 0; case _ => 0; }
  match (value) { case (int(declaration_pattern_constant)) => 0; case _ => 0; }
  match (value) { case int() => 0; case _ => 0; }
  match (value) { case (int()) => 0; case _ => 0; }
  match (value) { case (int named) => named; }

  struct Owner { int member; };
  int Owner::*member = &Owner::member;
  match (member) { case int Owner::*pointer => pointer == member; }

  using Function = int(double);
  Function *function = nullptr;
  match (function) { case Function* pointer => pointer == function; }

  using Array = int[2];
  Array array{};
  match (array) { case Array& reference => &reference == &array; }
}

void test_direct_function_and_array_declarators_are_not_patterns() {
  using Function = int(double);
  Function *function = nullptr;
  match (function) {
    case int (*copy)(double) => 0; // expected-error {{use of undeclared identifier 'copy'}} expected-error {{expected '(' for function-style cast or type construction}}
    case _ => 0;
  }

  using Array = int[2];
  Array array{};
  match (array) {
    case int (&copy)[2] => 0; // expected-error {{use of undeclared identifier 'copy'}}
    case _ => 0;
  }
}

void test_decomposition_pattern() {
  struct Empty {};
  Empty empty;
  match (empty) { case [] => 0; }
  int nested_single[1][1] = { { 1 } };
  nested_single match case [[_]];
  int xs[2] = { 1, 2 };
  xs match case [_, _];
  xs match case [_, 3];
  xs match case [1, 2];
  xs match case [...];
  int xss[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  xss match case [[_, _, _], [_, _, _]];
  xss match case [[1, _, _], [4, 5, _]];
}

void test_attributed_declaration_pattern(int value) {
  match (value) {
    case [[maybe_unused]] int copy => copy;
  }
  match (value) { case int copy [[maybe_unused]] => copy; }

  int pair[2] = {1, 2};
  match (pair) {
    [[likely]] case [[maybe_unused]] auto [first, second] => first + second;
  }

  int nested[1][2] = {{1, 2}};
  match (nested) {
    [[likely]] case [[maybe_unused]] auto [[first, second]] => first + second;
  }
}

void test_invalid_decomposition_pattern() {
  struct S { int a; int b; };
  S s{1, 2};
  match (s) { case [0,] => 0; case _ => 0; } // expected-error {{expected expression}}
  match (s) { case [0,,] => 0; case _ => 0; } // expected-error {{expected expression}}
  match (s) { case [0 0] => 0; case _ => 0; } // expected-error {{expected ']'}} expected-error {{type 'S' binds to 2 elements, but only 1 name was provided}} expected-note {{to match this '['}}
  match (s) { case [,] => 0; case _ => 0; } // expected-error {{expected expression}}
  match (s) { case [int first, ..._, int last] => 0; } // expected-error {{expected ']'}} expected-note {{to match this '['}}
  match (s) { case [int first, ...42, int last] => 0; } // expected-error {{expected ']'}} expected-note {{to match this '['}}
  match (s) { case [int first, ...[_, _], int last] => 0; } // expected-error {{expected ']'}} expected-note {{to match this '['}}
  match (s) { case [int first, (...), int last] => 0; } // expected-error {{expected expression}}
  match (s) { case [int first, (auto&& ...middle), int last] => 0; } // expected-error {{expected ')'}} expected-note {{to match this '('}}
}

void test_parenthesized_pattern(int a, int b) {
  int _ = 0;
  a match case auto(_ + 1);
  a match case auto((a) + b);
  a match case auto(a = b);
  a match case auto(a ? b : 0);
  a match case auto((a, b));
  a match case auto([] { return 1; }());
  a match case auto(({ int value = 1; value; }));
  a match case ([[maybe_unused]] int value);

  match (a) {
    case (a + b) => 0;
    case (_) => 0;
  }

  PatternPair pair{};
  match (pair) {
    case ([0, int y]) => y;
    case _ => 0;
  }
}

void test_pattern_introducers_commit(int value) {
  int _ = 0;
  match (value) {
    case _ + 1 => 0; // expected-error {{expected '=>' after pattern}}
    case _ => 0;
  }

  match (value) {
    case (value) + 1 => 0; // expected-error {{expected '=>' after pattern}}
    case _ => 0;
  }

  struct Empty {};
  Empty empty;
  match (empty) {
    case [] {}() => 0; // expected-error {{expected '=>' after pattern}}
    case _ => 0;
  }
}

int test_structured_jump_statements(char c) {
  foo:
  match (c) {
    case 'a' => break;        // expected-error {{'break' statement not in loop or switch statement}}
    case 'b' => continue;     // expected-error {{'continue' statement not in loop statement}}
    case 'c' => return;       // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
    case 'd' => return 42;
    case 'e' => co_return 42; // expected-error {{std::coroutine_traits type was not found}}
    case 'f' => goto foo;
    case _ => 0;
  }

  while (true) {
    match (c) {
      case 'a' => break;
      case 'b' => continue;
      case 'c' => return;     // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
      case 'd' => return 42;
      case 'e' => goto foo;
      case _ => 0;
    }
  }
}

void test_jump_into_match_handler(int value) {
  goto handler; // expected-error {{cannot jump from this goto statement to its label}}
  match (value) {
    case int copy => handler: (void)copy; // expected-note {{jump enters a match handler}}
  }
}

void test_switch_into_match_handler(int value) {
  switch (value) {
    match (value) {
      case _ => case 0: break; // expected-error {{cannot jump from switch statement to this case label}} expected-note {{jump enters a match handler}}
    }
  }
}

void test_jump_from_guard_into_match_handler(int value) {
  match (value) {
    case int copy if (({ goto handler; false; })) => // expected-error {{cannot jump from this goto statement to its label}}
      handler: (void)copy; // expected-note {{jump enters a match handler}}
    case _ => ;
  }
}

void test_deduced_return_type(int x) {
  (void)match (x) {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };

  (void)match (x) -> auto {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };

  (void)match (x) -> decltype(auto) {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'decltype(auto)' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'decltype(auto)' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'decltype(auto)' in return type deduced as 'const char (&)[6]' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };
}

void test_trailing_return_type(int x) {
  (void)match (x) -> int {
    case 0 => 0;
    case 1 => 0.0;
    case 2 => 'c';
    case _ => 0;
  };
}

bool test_match_test_with_guard(const int (&xs)[2]) {
  bool result = xs match case auto&& [x, y] if (x == y);
  bool init_result =
      xs match case auto&& [x, y] if (int sum = x + y; sum == 0);
  x; // expected-error {{use of undeclared identifier 'x'}}
  y; // expected-error {{use of undeclared identifier 'y'}}
  sum; // expected-error {{use of undeclared identifier 'sum'}}
  if (xs match case auto&& [x, y] if (int sum = x + y; sum == 0)) {
    x; // expected-error {{use of undeclared identifier 'x'}}
    y; // expected-error {{use of undeclared identifier 'y'}}
    sum; // expected-error {{use of undeclared identifier 'sum'}}
  } else {
    sum; // expected-error {{use of undeclared identifier 'sum'}}
  }
  return result && init_result;
}

int test_match_select_with_guards(const int (&p)[2]) {
  return match (p) {
    case auto&& [x, y] if (x < 0 && y < 0) => 0;
    case auto&& [x, y] if (x < 0) => y;
    case auto&& [x, y] if (bool b = y < 0) => [&] {
      y;
      b;
      return x;
    }();
    case auto&& [x, y] if (int sum = x + y; sum < 0) => sum;
    case auto&& [x, y] => x + y;
  };
}

void test_match_in_condition(const int *p, const int (*q)[2]) {
  p match case { auto&& v };
  v; // expected-error {{use of undeclared identifier 'v'}}
  if (p match case { auto&& v }) v; // expected-error {{use of undeclared identifier 'v'}}
  else v; // expected-error {{use of undeclared identifier 'v'}}
  if (p match case { auto&& v })
    int v;
  else
    int v;
  if (p match case { auto&& v }) {
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (int i = 0; p match case { auto&& v }) {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (p match case { auto&& v }) {
    int v;
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match case { auto&& v }) {
    int i; // expected-error {{redefinition of 'i'}}
    int v;
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match case { auto&& v }) {
    int v;
  } else {
    int i; // expected-error {{redefinition of 'i'}}
    int v;
  }
  if ((p match case { auto&& v })) {
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (int i = 0; (p match case { auto&& v })) {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (!(p match case { auto&& v })) {
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (q match case { [0, auto&& v] } match case auto&& w) {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match case { 0 } match case auto&& w) {
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match case { (0 match case auto&& w) }) {
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (q match case { [auto&& v, auto&& w] }) {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (q match case { [auto&& v, auto&& w] } + 1) {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  auto next = []() -> int* { return nullptr; };
  for (int i = 0; next() match case { auto&& elem }; ++i)
    elem; // expected-error {{use of undeclared identifier 'elem'}}
  for (int i = 0; next() match case { auto&& elem }; ++i)
    int elem;
  for (int i = 0; next() match case { auto&& elem }; ++i) {
    elem; // expected-error {{use of undeclared identifier 'elem'}}
  }
  for (int i = 0; next() match case { auto&& elem }; ++i) {
    int elem;
  }
  while (next() match case { auto&& elem })
    elem; // expected-error {{use of undeclared identifier 'elem'}}
  while (next() match case { auto&& elem })
    int elem;
  while (next() match case { auto&& elem }) {
    elem; // expected-error {{use of undeclared identifier 'elem'}}
  }
  while (next() match case { auto&& elem }) {
    int elem;
  }

  auto f = [](int x, int y) { return true; };
  if (q match case { [auto&& x, auto&& y] } if (bool b = f(x, y))) {
    x; // expected-error {{use of undeclared identifier 'x'}}
    y; // expected-error {{use of undeclared identifier 'y'}}
    b; // expected-error {{use of undeclared identifier 'b'}}
  }
}

void test_case_condition(int value, const int (&pair)[2]) {
  if (case int copy = value) {
    copy;
  } else {
    copy; // expected-error {{use of undeclared identifier 'copy'}}
  }
  copy; // expected-error {{use of undeclared identifier 'copy'}}

  while (case int copy = value) {
    copy;
    break;
  }

  for (int count = 0; case int copy = value; ++copy, ++count) {
    copy;
    count;
    break;
  }

  if (case [int first, int second] = pair) {
    first;
    second;
  }

  if (value > 0 && case int copy = value && copy < 10) {
    copy;
  } else {
    copy; // expected-error {{use of undeclared identifier 'copy'}}
  }

  if (case int first = value && case int second = first + 1 &&
      second > first) {
    first;
    second;
  }

  while (value > 0 && case int copy = value && copy < 10) {
    copy;
    break;
  }

  for (int count = 0;
       count < 1 && case int copy = value && copy < 10;
       ++count, ++copy) {
    copy;
  }
}

void test_case_condition_subject_grammar(int lhs, int rhs) {
  if (case int value = lhs | rhs && value > 0) {}
  if (case auto value = (lhs || rhs) && value) {}
  if (case int value = (lhs = rhs) && value > 0) {}
  if (case int value = (lhs ? rhs : lhs) && value > 0) {}

  if (case int value = lhs || rhs) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}
  if (case int value = lhs = rhs) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}
  if (case int value = lhs ? rhs : lhs) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}
  if (lhs || rhs && case int value = lhs) {} // expected-error {{expected expression}}
}

void test_case_condition_is_direct_only(int value) {
  if ((case int copy = value)) {} // expected-error {{expected expression}}
  bool result = case int copy = value; // expected-error {{expected expression}}
  switch (case int copy = value) {} // expected-error {{a pattern condition is not permitted in a switch statement}}
}

void test_case_condition_is_not_an_init_statement(int value) {
  if (case int copy = value; copy > 0) {} // expected-error {{a pattern condition cannot be used as an init-statement}}
}

void test_case_condition_is_not_a_switch_init_statement(int value) {
  switch (case int copy = value; copy) {} // expected-error {{a pattern condition cannot be used as an init-statement}}
}

void test_case_condition_is_not_a_for_init_statement(int value) {
  for (case int copy = value; copy > 0; ++copy) {} // expected-error {{a pattern condition cannot be used as an init-statement}}
}

int test_case_condition_is_not_a_guard_init_statement(int value) {
  return match (value) {
    case int copy if (case int nested = copy; nested > 0) => 1; // expected-error {{a pattern condition cannot be used as an init-statement}}
    case _ => 0;
  };
}

int test_attributed_cases(int value) {
  return match (value) {
    [[likely]] case 0 => 1;
    [[unlikely]] case _ => 2;
  };
}

int test_default_is_not_a_match_arm(int value) {
  return match (value) {
    default => 1; // expected-error {{expected 'case' before pattern}}
  };
}

int test_unknown_case_attribute(int value) {
  return match (value) {
    [[unknown_match_case_attribute]] case _ => 0; // expected-warning {{unknown attribute 'unknown_match_case_attribute' ignored}}
  };
}

template <int... Is, int N>
int test_pack_expansion_in_decomposition_pattern(const int (&p)[N]) {
  return match (p) {
    case [0, Is...] => 0;
    case [Is..., 0] => 1;
    case _ => -1;
  };
}

template <int... Is, int N>
void test_non_pattern_pack_expansion(const int (&p)[N]) {
  p match { case [(Is...)] => 0; }; // expected-error {{expected ')'}} expected-note {{to match this '('}}
  p match case auto&& ...elements; // expected-error {{expected expression}}
  p match case Is...; // expected-error {{expected expression}}
}
