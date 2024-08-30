// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -Wno-unused-value -verify %s

void test_match_is_not_keyword() {
  int match;
  (void)match;
  int foo(int match);
  {
    struct match {};
    match match;
    (void)match;
  }
}

void test_let_is_not_keyword() {
  int let;
  (void)let;
  int foo(int let);
  {
    struct let {};
    let let;
    (void)let;
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
  x match ? _;
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
  x match { ? _ => 0; _ => 1; };
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
  }
  i match { let x => 0; };
  i match { let [x] => 0; };
  i match { let [x, y] => 0; };
  i match { [let x, let y] => 0; };
}

void test_optional_pattern(int* p) {
  p match ? _;
  p match ? 0;
  p match { ? _ => 0; };
  p match { ?? _ => 0; };
  p match { ??? 1 => 0; };
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
