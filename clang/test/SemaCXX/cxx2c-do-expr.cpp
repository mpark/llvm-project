// RUN: %clang_cc1 -verify -std=c++2c %s -fsyntax-only

int x = do { do_return 42; }; // expected-error {{do expression not allowed at file scope}}

void f() {
  do {            // expected-note {{to match this 'do'}}
    do_return 42; // expected-error {{'do_return' statement not in do expression}}
  };              // expected-error {{expected 'while' in do/while loop}}
}
