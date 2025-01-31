// RUN: %clang_cc1 -verify -std=c++2c %s -fsyntax-only

void f() {
  int i1 = do { do_return 42; };
  long l = do -> long { do_return 42; };
  int i3 = do -> { do_return 42; }; // expected-error {{expected a type}}
  int i4 = do -> i { do_return 42; }; // expected-error {{unknown type name 'i'}}

  int i5 = do {
    int y = i4;
    if (y > 0) {
        do_return y;
    } else {
        do_return -y;
    }
  };
}
