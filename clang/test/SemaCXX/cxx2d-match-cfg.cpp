// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wreturn-type -Wuninitialized \
// RUN:   -verify %s

// expected-no-diagnostics

int exhaustive(bool value) {
  value match {
    true => return 1;
    false => return 0;
  };
}

int guarded(int value) {
  return value match {
    let copy if (copy > 0) => copy;
    _ => 0;
  };
}

int guarded_init_statement(int value) {
  return value match {
    let copy if (int adjusted = copy + 1; adjusted > 0) => adjusted;
    _ => 0;
  };
}
