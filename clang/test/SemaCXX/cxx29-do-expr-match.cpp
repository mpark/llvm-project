// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

// expected-no-diagnostics

constexpr int match_handler_do_return(int value) {
  return do {
    value match -> void {
      case 0 => do_return 42;
      case _ => do_return 7;
    };
  };
}

static_assert(match_handler_do_return(0) == 42);
static_assert(match_handler_do_return(1) == 7);
