// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wreturn-type -Wuninitialized \
// RUN:   -verify %s

// expected-no-diagnostics

int exhaustive(bool value) {
  value match {
    case true => return 1;
    case false => return 0;
  };
}

int guarded(int value) {
  return value match {
    case let copy if (copy > 0) => copy;
    case _ => 0;
  };
}

int declaration(int value) {
  value match {
    case int copy => return copy;
  };
}

int guarded_init_statement(int value) {
  return value match {
    case let copy if (int adjusted = copy + 1; adjusted > 0) => adjusted;
    case _ => 0;
  };
}
