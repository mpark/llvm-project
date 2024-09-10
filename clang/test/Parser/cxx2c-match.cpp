// RUN: %clang_cc1 -std=c++23 -fsyntax-only -fpattern-matching -Wno-unused-variable -Wno-unused-value -verify %s

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
  x match { _ if true => 0; };
  x match constexpr { _ => 0; };
  x match constexpr { _ if true => 0; };
  x match -> int { _ => 0; };
  x match -> auto { _ => 0; };
  x match -> decltype(auto) { _ => 0; };
  x match -> int { _ if true => 0; };
  x match -> auto { _ if true => 0; };
  x match -> decltype(auto) { _ if true => 0; };
  x match constexpr -> int { _ => 0; };
  x match constexpr -> auto { _ => 0; };
  x match constexpr -> decltype(auto) { _ => 0; };
  x match constexpr -> int { _ if true => 0; };
  x match constexpr -> auto { _ if true => 0; };
  x match constexpr -> decltype(auto) { _ if true => 0; };
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
}

void test_binding_pattern(int i) {
  i match { let => 0; }; // expected-error {{expected identifier or '['}}
  {
    int let = 42;
    i match { (let) => 0; };
  }
  {
    constexpr int let[2] = {1, 2};
    constexpr int idx = 0;
    i match { (let[idx]) => 0; };
    // i match { (let x) => 0; };
  }
  i match let x;
  x; // expected-error {{use of undeclared identifier 'x'}}
  i match { let x => 0; };
  i match { let x => x; };
  i match { let [x] => 0; };
  i match { let [x] => x; };
  i match { let [x, y] => 0; };
  i match { let [x, y] => x + y; };
  i match { [let x, let y] => 0; };
  i match { [let x, let y] =>  x + y; };
}

void test_optional_pattern(int* p) {
  p match ? _;
  p match ? 0;
  p match { ? _ => 0; };
  int **pp = &p;
  pp match { ?? _ => 0; };
  &pp match { ??? 1 => 0; };
}

void test_decomposition_pattern() {
  int xs[2] = { 1, 2 };
  xs match [_, _];
  xs match [_, 3];
  xs match [1, 2];
  int xss[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  xs match [[_, _, _], [_, _, _]];
  xs match [[1, _, _], [4, 5, _]];
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
