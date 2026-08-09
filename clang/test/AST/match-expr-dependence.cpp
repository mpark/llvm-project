// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

template <class T>
void test_match_dependence(int x, T value) {
  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match case T;

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match case _ if (sizeof(T) != 0)

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match case _ if (T guard = value; true)

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match case _ if (T guard = value)

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match { case _ if (T guard = value; true) => 0; case _ => 1; }

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match { case _ if (T guard = value) => 0; case _ => 1; }

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match { case sizeof(T) => 0; case _ => 1; }

  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match -> unsigned { case _ => sizeof(T); }

  // expected-warning@+1 {{type-dependent expression}}
#pragma clang __debug dump x match -> T { case _ => value; }
}

template <class T>
T test_statement_handler_dependence(int x, T value) {
  // expected-warning@+1 {{value-dependent expression}}
#pragma clang __debug dump x match { case _ => return value; }
}
