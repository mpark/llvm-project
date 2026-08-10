// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -Wno-unused-variable -Wno-unused-value -verify %s

namespace std {
template <class T>
struct alternative_traits;

template <class T>
struct alternative_traits<T *> {
  static constexpr __SIZE_TYPE__ size = 2;

  template <__SIZE_TYPE__ I>
    requires(I == 0)
  using projection_type = T;

  static constexpr __SIZE_TYPE__ index(T *pointer) noexcept {
    return pointer ? 0 : 1;
  }

  template <__SIZE_TYPE__ I, class Self>
    requires(I == 0)
  static constexpr decltype(auto) get(Self &&self) {
    return *self;
  }

  static consteval __SIZE_TYPE__ index_of(decltype(nullptr)) { return 1; }
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
  42 match constexpr; // expected-error {{expected '{'}}
  42 match -> ; // expected-error {{expected a type}}
  42 match->i; // expected-error {{unknown type name 'i'}}
  42 match -> void; // expected-error {{expected '{'}}
}

void test_case_is_required(int x) {
  x match _; // expected-error {{expected case}}
  x match { _ => 0; case _ => 0; }; // expected-error {{expected case}}
}

void test_match_structures(int x) {
  x match case _;
  &x match case { _ };
  x match case 0;
  x match { case _ => 0; };
  x match { case _ if (true) => 0; case _ => 0; };
  x match constexpr { case _ => 0; };
  x match constexpr { case _ if (true) => 0; case _ => 0; };
  x match -> int { case _ => 0; };
  x match -> auto { case _ => 0; };
  x match -> decltype(auto) { case _ => 0; };
  x match -> int { case _ if (true) => 0; case _ => 0; };
  x match -> auto { case _ if (true) => 0; case _ => 0; };
  x match -> decltype(auto) { case _ if (true) => 0; case _ => 0; };
  x match constexpr -> int { case _ => 0; };
  x match constexpr -> auto { case _ => 0; };
  x match constexpr -> decltype(auto) { case _ => 0; };
  x match constexpr -> int { case _ if (true) => 0; case _ => 0; };
  x match constexpr -> auto { case _ if (true) => 0; case _ => 0; };
  x match constexpr -> decltype(auto) { case _ if (true) => 0; case _ => 0; };
  &x match { case { _ } => 0; case _ => 1; };
}

struct PatternPair {
  int first;
  int second;
};

int test_declaration_pattern_before_comma(PatternPair pair) {
  return pair match {
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
    *p match { case _ => 0; };
    *p match { case _ => 0; } + 1;
    // match binds tighter than bin ops.
    4 + 2 match { case _ => 0; };
    4 * 2 match { case _ => 0; };
    4 == 2 match { case _ => 0; };
    4 * (2) match { case _ => 0; };
    2 match { case _ => 0; } + 1;
    2 match { case _ => 0; } * 1;
    2 match { case _ => 0; } == 1;
    (2) match { case _ => 0; } * 1;
    // except .* and ->*
    struct S { int i; } s;
    s.*&S::i match { case _ => 0; };
    &s->*&S::i match { case _ => 0; };
    2 match { case _ => s; } .* &S::i;
    2 match { case _ => &s; } ->* &S::i;
    // unary parenthesized
    !(p match { case _ => 0; });
    !((p) match { case _ => 0; });
  }
}

void test_wildcard_pattern(int x) {
  x match case _;
  bool b = x match case _;
  x match { case _ => 0; };
}

void test_expression_pattern(int x, int y) {
  x match case 0;
  x match case (1 + 2);
  x match case y;
  int _ = 0;
  x match case +_;
  x match case -_;
  x match case y + 1;
  x match case _ + 1;
  x match {
    case y + 1 => 0;
    case _ + 1 => 0; // expected-error {{expected '=>' after pattern}}
    case _ => 0;
  };
  x match case (int)y;
  using Int = int;
  x match case (Int)y;
  x match case (Int)(y);
  x match case (((Int)(y)));
  constexpr auto id = [](auto &&x) -> auto && {
    return static_cast<decltype(x)>(x);
  };
  {
    int let = 42;
    x match case id(let);
    x match { case id(let) => 0; case _ => 0; };
  }
  {
    constexpr int let[2] = {1, 2};
    constexpr int idx = 0;
    x match { case id(let[idx]) => 0; case _ => 0; };
  }
  x match {
    case y++ => 0;
    case y++ * 2 => 0;
    case (y++) => 0;
    case (y)++ * 2 => 0;
    case _ => 0;
  };
}

void test_declaration_pattern(int i) {
  i match case auto&& x;
  x; // expected-error {{use of undeclared identifier 'x'}}
  i match { case auto&& x => 0; };
  i match { case auto&& x => x; };
  i match { case auto&& [x] => 0; case _ => 0; }; // expected-error {{cannot bind non-class, non-array type 'int'}}
  int i1[1] = {0};
  i1 match { case auto&& [x] => 0; };
  i1 match { case auto&& [x] => x; };
  int i2[2] = {0, 0};
  i2 match { case auto&& [x, y] => 0; };
  i2 match { case auto&& [x, y] => x + y; };
  i2 match { case [auto&& x, auto&& y] => 0; };
  i2 match { case [auto&& x, auto&& y] =>  x + y; };
}

template <class T>
concept Integral = __is_integral(T);

void test_constrained_declaration_pattern(int i) {
  i match case Integral auto value;
  i match { case Integral auto value => value; };
}

void test_decomposition_pattern() {
  int nested_single[1][1] = { { 1 } };
  nested_single match case [[_]];
  int xs[2] = { 1, 2 };
  xs match case [_, _];
  xs match case [_, 3];
  xs match case [1, 2];
  int xss[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  xss match case [[_, _, _], [_, _, _]];
  xss match case [[1, _, _], [4, 5, _]];
}

void test_attributed_declaration_pattern(int value) {
  value match {
    case [[maybe_unused]] int copy => copy;
  };
}

void test_invalid_decomposition_pattern() {
  struct S { int a; int b; };
  S s{1, 2};
  s match { case [] => 0; case _ => 0; }; // expected-error {{expected expression}}
  s match { case [0,] => 0; case _ => 0; }; // expected-error {{expected expression}}
  s match { case [0,,] => 0; case _ => 0; }; // expected-error {{expected expression}}
  s match { case [0 0] => 0; case _ => 0; }; // expected-error {{expected ']'}} expected-error {{type 'S' binds to 2 elements, but only 1 name was provided}} expected-note {{to match this '['}}
  s match { case [,] => 0; case _ => 0; }; // expected-error {{expected expression}}
}

void test_parenthesized_expression_pattern(int a, int b) {
  a match {
    case (a) + b => 0;
    case _ => 0;
  };
}

int test_structured_jump_statements(char c) {
  foo:
  c match {
    case 'a' => break;        // expected-error {{'break' statement not in loop or switch statement}}
    case 'b' => continue;     // expected-error {{'continue' statement not in loop statement}}
    case 'c' => return;       // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
    case 'd' => return 42;
    case 'e' => co_return 42; // expected-error {{std::coroutine_traits type was not found}}
    case 'f' => goto foo;     // expected-error {{cannot jump from this goto statement to its label}}
    case _ => 0;
  };

  while (true) {
    c match {
      case 'a' => break;
      case 'b' => continue;
      case 'c' => return;     // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
      case 'd' => return 42;
      case 'e' => goto foo;   // expected-error {{cannot jump from this goto statement to its label}}
      case _ => 0;
    };
  }
}

void test_deduced_return_type(int x) {
  x match {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };

  x match -> auto {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };

  x match -> decltype(auto) {
    case 0 => 0;
    case 1 => 0.0;     // expected-error {{'decltype(auto)' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    case 2 => 'c';     // expected-error {{'decltype(auto)' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    case 3 => "hello"; // expected-error {{'decltype(auto)' in return type deduced as 'const char (&)[6]' here but deduced as 'int' in earlier return statement}}
    case _ => 0;
  };
}

void test_trailing_return_type(int x) {
  x match -> int {
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
    x;
    y;
    sum;
  } else {
    sum; // expected-error {{use of undeclared identifier 'sum'}}
  }
  return result && init_result;
}

int test_match_select_with_guards(const int (&p)[2]) {
  return p match {
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
  if (p match case { auto&& v }) v;
  else v; // expected-error {{use of undeclared identifier 'v'}}
  if (p match case { auto&& v }) // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v'}}
  else
    int v;
  if (p match case { auto&& v }) {
    v;
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (int i = 0; p match case { auto&& v }) {
    i;
    v;
  } else {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (p match case { auto&& v }) { // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v'}}
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match case { auto&& v }) { // expected-note {{previous definition is here}}
    int i; // expected-error {{redefinition of 'i'}}
    int v; // expected-error {{redefinition of 'v'}}
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match case { auto&& v }) { // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v'}}
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
    w;
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match case { 0 } match case auto&& w) {
    w;
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match case { (0 match case auto&& w) }) {
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (q match case { [auto&& v, auto&& w] }) {
    v;
    w;
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
  for (int i = 0; next() match case { auto&& elem }; ++i) elem;
  for (int i = 0;
       next() match case { auto&& elem }; // expected-note {{previous definition is here}}
       ++i)
    int elem; // expected-error {{redefinition of 'elem'}}
  for (int i = 0; next() match case { auto&& elem }; ++i) {
    elem;
  }
  for (int i = 0;
       next() match case { auto&& elem }; // expected-note {{previous definition is here}}
       ++i) {
    int elem; // expected-error {{redefinition of 'elem'}}
  }
  while (next() match case { auto&& elem }) elem;
  while (next() match case { auto&& elem }) // expected-note {{previous definition is here}}
    int elem; // expected-error {{redefinition of 'elem'}}
  while (next() match case { auto&& elem }) {
    elem;
  }
  while (next() match case { auto&& elem }) { // expected-note {{previous definition is here}}
    int elem; // expected-error {{redefinition of 'elem'}}
  }

  auto f = [](int x, int y) { return true; };
  if (q match case { [auto&& x, auto&& y] } if (bool b = f(x, y))) {
    x;
    y;
    b;
  }
}

void test_case_condition(int value, const int (&pair)[2]) {
  if (case int copy = value) {
    copy;
  } else {
    copy; // expected-error {{use of undeclared identifier 'copy'}}
  }
  copy; // expected-error {{use of undeclared identifier 'copy'}}

  if (int init = 1;
      case int copy = value if (int sum = init + copy; sum > 0)) {
    init;
    copy;
    sum;
  } else {
    init;
    copy; // expected-error {{use of undeclared identifier 'copy'}}
    sum;  // expected-error {{use of undeclared identifier 'sum'}}
  }

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
}

void test_case_condition_is_direct_only(int value) {
  if ((case int copy = value)) {} // expected-error {{expected expression}}
  bool result = case int copy = value; // expected-error {{expected expression}}
  switch (case int copy = value) {} // expected-error {{expected expression}}
}

template <int... Is, int N>
int test_pack_expansion_in_decomposition_pattern(const int (&p)[N]) {
  return p match {
    case [0, Is...] => 0;
    case [Is..., 0] => 1;
    case _ => -1;
  };
}
