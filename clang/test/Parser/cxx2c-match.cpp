// RUN: %clang_cc1 -std=c++2c -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value -verify %s

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
  42 match; // expected-error {{expected expression}}
            // TODO: improve this error message.
  42 match constexpr; // expected-error {{expected '{'}}
  42 match -> ; // expected-error {{expected a type}}
  42 match->i; // expected-error {{unknown type name 'i'}}
  42 match -> void; // expected-error {{expected '{'}}
}

void test_match_structures(int x) {
  x match _;
  &x match ? _;
  x match 0;
  x match { _ => 0; };
  x match { _ if (true) => 0; };
  x match constexpr { _ => 0; };
  x match constexpr { _ if (true) => 0; };
  x match -> int { _ => 0; };
  x match -> auto { _ => 0; };
  x match -> decltype(auto) { _ => 0; };
  x match -> int { _ if (true) => 0; };
  x match -> auto { _ if (true) => 0; };
  x match -> decltype(auto) { _ if (true) => 0; };
  x match constexpr -> int { _ => 0; };
  x match constexpr -> auto { _ => 0; };
  x match constexpr -> decltype(auto) { _ => 0; };
  x match constexpr -> int { _ if (true) => 0; };
  x match constexpr -> auto { _ if (true) => 0; };
  x match constexpr -> decltype(auto) { _ if (true) => 0; };
  &x match { ? _ => 0; _ => 1; };
}

void test_match_precedence(int* p) {
  /* MatchTestExpr */ {
    // unary is tighter than match
    *p match 0;
    *p match 0 + 1;
    // match binds tighter than bin ops.
    4 + 2 match 0;
    4 * 2 match 0;
    true == 2 match 0;
    4 * (2) match 0;
    2 match 0 + 1;
    2 match 0 * 1;
    2 match 0 == 1;
    (2) match 0 * 1;
    // except .* and ->*
    struct S { int i; } s;
    s.*&S::i match 0;
    &s->*&S::i match 0;
    2 match s.*&S::i;
    2 match &s->*&S::i;
    // unary parenthesized
    !(p match nullptr);
    !((p) match nullptr);
  }
  /* MatchSelectExpr */ {
    // unary is tighter than match
    *p match { _ => 0; };
    *p match { _ => 0; } + 1;
    // match binds tighter than bin ops.
    4 + 2 match { _ => 0; };
    4 * 2 match { _ => 0; };
    4 == 2 match { _ => 0; };
    4 * (2) match { _ => 0; };
    2 match { _ => 0; } + 1;
    2 match { _ => 0; } * 1;
    2 match { _ => 0; } == 1;
    (2) match { _ => 0; } * 1;
    // except .* and ->*
    struct S { int i; } s;
    s.*&S::i match { _ => 0; };
    &s->*&S::i match { _ => 0; };
    2 match { _ => s; } .* &S::i;
    2 match { _ => &s; } ->* &S::i;
    // unary parenthesized
    !(p match { _ => 0; });
    !((p) match { _ => 0; });
  }
}

void test_wildcard_pattern(int x) {
  x match _;
  bool b = x match _;
  x match { _ => 0; };
}

void test_expression_pattern(int x, int y) {
  x match 0;
  x match (1 + 2);
  x match y;
  int _ = 0;
  x match +_;
  x match -_;
  x match y + 1;
  x match _ + 1;
  x match {
    y + 1 => 0;
    _ + 1 => 0; // expected-error {{expected '=>' after pattern}}
    _ => 0;
  };
  x match (int)y;
  using Int = int;
  x match (Int)y;
  x match (Int)(y);
  x match (((Int)(y)));
  constexpr auto id = [](auto &&x) -> auto && {
    return static_cast<decltype(x)>(x);
  };
  {
    int let = 42;
    x match id(let);
    x match { id(let) => 0; };
  }
  {
    constexpr int let[2] = {1, 2};
    constexpr int idx = 0;
    x match { id(let[idx]) => 0; };
  }
  x match {
    y++ => 0;
    y++ * 2 => 0;
    (y++) => 0;
    (y)++ * 2 => 0;
    _ => 0;
  };
}

void test_binding_pattern(int i) {
  i match { let => 0; }; // expected-error {{expected identifier or '['}}
  i match let x;
  x; // expected-error {{use of undeclared identifier 'x'}}
  i match { let x => 0; };
  i match { let x => x; };
  i match { let [x] => 0; }; // expected-error {{cannot decompose non-class, non-array type 'int'}}
  int i1[1] = {0};
  i1 match { let [x] => 0; };
  i1 match { let [x] => x; };
  int i2[2] = {0, 0};
  i2 match { let [x, y] => 0; };
  i2 match { let [x, y] => x + y; };
  i2 match { [let x, let y] => 0; };
  i2 match { [let x, let y] =>  x + y; };
}

void test_optional_pattern(int *p) {
  p match ? _;
  p match ? 0;
  p match { ? _ => 0; };
  int **pp = &p;
  pp match { ?? _ => 0; };
  &pp match { ??? 1 => 0; };
}

void test_alternative_pattern() {
  struct Base { virtual ~Base() = default; };
  struct Derived : Base {};

  Derived d;
  Base &b = d;
  b match Derived: _;
  b match {
    Derived: let x => 0;
    _ => 0;
  };
}

void test_decomposition_pattern() {
  int xs[2] = { 1, 2 };
  xs match [_, _];
  xs match [_, 3];
  xs match [1, 2];
  int xss[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  xss match [[_, _, _], [_, _, _]];
  xss match [[1, _, _], [4, 5, _]];
}

void test_paren_pattern(int *p, int a, int b) {
  p match { (let x) => 0; };
  p match { ? (let x) => 0; };
  int **pp = &p;
  pp match { ?(? _) => 0; };
  pp match { ?(? (let x)) => 0; };
  p match {
    ? (a) + b => 0;
  };
}

int test_structured_jump_statements(char c) {
  foo:
  c match {
    'a' => break;        // expected-error {{'break' statement not in loop or switch statement}}
    'b' => continue;     // expected-error {{'continue' statement not in loop statement}}
    'c' => return;       // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
    'd' => return 42;
    'e' => co_return 42; // expected-error {{std::coroutine_traits type was not found}}
    'f' => goto foo;     // expected-error {{cannot jump from this goto statement to its label}}
  };

  while (true) {
    c match {
      'a' => break;
      'b' => continue;
      'c' => return;     // expected-error {{non-void function 'test_structured_jump_statements' should return a value}}
      'd' => return 42;
      'e' => goto foo;   // expected-error {{cannot jump from this goto statement to its label}}
    };
  }
}

void test_deduced_return_type(int x) {
  x match {
    0 => 0;
    1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
  };

  x match -> auto {
    0 => 0;
    1 => 0.0;     // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    2 => 'c';     // expected-error {{'auto' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    3 => "hello"; // expected-error {{'auto' in return type deduced as 'const char *' here but deduced as 'int' in earlier return statement}}
  };

  x match -> decltype(auto) {
    0 => 0;
    1 => 0.0;     // expected-error {{'decltype(auto)' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
    2 => 'c';     // expected-error {{'decltype(auto)' in return type deduced as 'char' here but deduced as 'int' in earlier return statement}}
    3 => "hello"; // expected-error {{'decltype(auto)' in return type deduced as 'const char (&)[6]' here but deduced as 'int' in earlier return statement}}
  };
}

void test_trailing_return_type(int x) {
  x match -> int {
    0 => 0;
    1 => 0.0;
    2 => 'c';
  };
}

bool test_match_test_with_guard(const int (&xs)[2]) {
  bool result = xs match let [x, y] if (x == y);
  x; // expected-error {{use of undeclared identifier 'x'}}
  y; // expected-error {{use of undeclared identifier 'y'}}
  return result;
}

int test_match_select_with_guards(const int (&p)[2]) {
  return p match {
    let [x, y] if (x < 0 && y < 0) => 0;
    let [x, y] if (x < 0) => y;
    let [x, y] if (bool b = y < 0) => [&] {
      y;
      b;
      return x;
    }();
    let [x, y] => x + y;
  };
}

void test_match_in_condition(const int *p, const int (*q)[2]) {
  p match ? let v;
  v; // expected-error {{use of undeclared identifier 'v'}}
  if (p match ? let v) v;
  else v; // expected-error {{use of undeclared identifier 'v'}}
  if (p match ? let v) // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v' as different kind of symbol}}
  else
    int v;
  if (p match ? let v) {
    v;
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (int i = 0; p match ? let v) {
    i;
    v;
  } else {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (p match ? let v) { // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v' as different kind of symbol}}
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match ? let v) { // expected-note {{previous definition is here}}
    int i; // expected-error {{redefinition of 'i'}}
    int v; // expected-error {{redefinition of 'v' as different kind of symbol}}
  } else {
    int v;
  }
  if (int i = 0; // expected-note {{previous definition is here}}
      p match ? let v) { // expected-note {{previous definition is here}}
    int v; // expected-error {{redefinition of 'v' as different kind of symbol}}
  } else {
    int i; // expected-error {{redefinition of 'i'}}
    int v;
  }
  if ((p match ? let v)) {
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (int i = 0; (p match ? let v)) {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    i;
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (!(p match ? let v)) {
    v; // expected-error {{use of undeclared identifier 'v'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
  }
  if (q match ? [0, let v] match let w) {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w;
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match ? 0 match let w) {
    w;
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (p match ? (0 match let w)) {
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (q match ? let [v, w]) {
    v;
    w;
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  if (q match ? let [v, w] + 1) {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  } else {
    v; // expected-error {{use of undeclared identifier 'v'}}
    w; // expected-error {{use of undeclared identifier 'w'}}
  }
  auto next = []() -> int* { return nullptr; };
  for (int i = 0; next() match (? let elem); ++i) elem;
  for (int i = 0;
       next() match (? let elem); // expected-note {{previous definition is here}}
       ++i)
    int elem; // expected-error {{redefinition of 'elem' as different kind of symbol}}
  for (int i = 0; next() match (? let elem); ++i) {
    elem;
  }
  for (int i = 0;
       next() match (? let elem); // expected-note {{previous definition is here}}
       ++i) {
    int elem; // expected-error {{redefinition of 'elem' as different kind of symbol}}
  }
  while (next() match (? let elem)) elem;
  while (next() match (? let elem)) // expected-note {{previous definition is here}}
    int elem; // expected-error {{redefinition of 'elem' as different kind of symbol}}
  while (next() match (? let elem)) {
    elem;
  }
  while (next() match (? let elem)) { // expected-note {{previous definition is here}}
    int elem; // expected-error {{redefinition of 'elem' as different kind of symbol}}
  }

  auto f = [](int x, int y) { return true; };
  if (q match ? let [x, y] if (bool b = f(x, y))) {
    x;
    y;
    b;
  }
}

template <int... Is, int N>
int test_pack_expansion_in_decomposition_pattern(const int (&p)[N]) {
  return p match {
    [0, Is...] => 0;
    [Is..., 0] => 1;
    _ => -1;
  };
}
