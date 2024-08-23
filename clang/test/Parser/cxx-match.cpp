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

void test_match_no_rhs(int i) {
  42 match; // expected-error {{expected expression}}
            // TODO: improve this error message.
  42 match constexpr; // expected-error {{expected '{'}}
  42 match -> ; // expected-error {{expected a type}}
  42 match->i; // expected-error {{unknown type name 'i'}}
  42 match -> void; // expected-error {{expected '{'}}
}

void test_structures() {
  42 match { _ => 0; };
  42 match { _ if true => 0; };
  42 match constexpr { _ => 0; };
  42 match constexpr { _ if true => 0; };
  42 match -> int { _ => 0; };
  42 match -> int { _ if true => 0; };
  42 match constexpr -> int { _ => 0; };
  42 match constexpr -> int { _ if true => 0; };
}
