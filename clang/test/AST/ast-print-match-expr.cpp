// RUN: %clang_cc1 -std=c++2d -fpattern-matching -ast-print %s | FileCheck %s

namespace std {
template<class T> struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};

}

struct Pair {
  int first;
  long second;
};

struct Four {
  int first;
  int second;
  int third;
  int fourth;
};

struct Choice {
  unsigned state;
  int integer;
  double real;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {
    ^^int, ^^double
  };
  static constexpr bool has_residual_states = false;

  struct names {
    __SIZE_TYPE__ value;

    static constexpr __SIZE_TYPE__ integer = 0;
    static constexpr __SIZE_TYPE__ real = 1;

    template<__SIZE_TYPE__ I>
    static constexpr __SIZE_TYPE__ index = I;

    constexpr operator __SIZE_TYPE__() const noexcept { return value; }
    friend constexpr bool operator==(names, names) = default;
  };

  static constexpr names index(const Choice& value) noexcept {
    return {value.state};
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& value) {
    if constexpr (I == names::integer)
      return (static_cast<Self&&>(value).integer);
    else
      return (static_cast<Self&&>(value).real);
  }
};

template<class T>
concept Integral = __is_integral(T);

int select(Pair pair) {
  return match (pair) -> int {
    case [0, long second] if (second > 0) => second;
    case auto&& whole => 0;
  };
}

// CHECK-LABEL: int select(Pair pair) {
// CHECK-NEXT: {{^    }}return match (pair) -> int {
// CHECK-NEXT: {{^        }}case [0, long second] if (second > 0) => second;
// CHECK-NEXT: {{^        }}case auto &&whole => 0;
// CHECK-NEXT: {{^    }}};

void select_statement(Pair pair) {
  match (pair) {
    case [0, long second] => second;
    case _ => "other";
  }
}

// CHECK-LABEL: void select_statement(Pair pair) {
// CHECK-NEXT: {{^    }}match (pair) {
// CHECK-NEXT: {{^        }}case [0, long second] => second;
// CHECK-NEXT: {{^        }}case _ => "other";
// CHECK-NEXT: {{^    }}}
// CHECK-NEXT: {{^}}}

void select_compound_statement(Pair pair) {
  match (pair) {
    case [0, long second] => {
      long copy = second;
      ++copy;
    }
    case _ => ;
  }
}

// CHECK-LABEL: void select_compound_statement(Pair pair) {
// CHECK-NEXT: {{^    }}match (pair) {
// CHECK-NEXT: {{^        }}case [0, long second] => {
// CHECK-NEXT: {{^            }}long copy = second;
// CHECK-NEXT: {{^            }}++copy;
// CHECK-NEXT: {{^        }}}
// CHECK-NEXT: {{^        }}case _ => ;
// CHECK-NEXT: {{^    }}}
// CHECK-NEXT: {{^}}}

bool tests(Pair pair) {
  bool direct = match(pair, case [int first, long second] if (first < second));
  if (case [int first, long second] = pair)
    return first < second;
  return direct;
}

// CHECK-LABEL: bool tests(Pair pair) {
// CHECK-NEXT: {{^    }}bool direct = match(pair, case [int first, long second] if (first < second));
// CHECK-NEXT: {{^    }}if (case [int first, long second] = pair)
// CHECK-NEXT: {{^        }}return first < second;
// CHECK-NEXT: {{^    }}return direct;

bool test_multiple(int first, int second) {
  return match(first, second, case [0, int value] if (value > 0));
}

// CHECK-LABEL: bool test_multiple(int first, int second) {
// CHECK-NEXT: {{^    }}return match(first, second, case [0, int value] if (value > 0));

int alternatives(Choice choice) {
  return match (choice) {
    case { .integer: int value } => value;
    case { .real: double } => 0;
  };
}

// CHECK-LABEL: int alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return match (choice) {
// CHECK-NEXT: {{^        }}case { .integer: int value } => value;
// CHECK-NEXT: {{^        }}case { .real: double } => 0;
// CHECK-NEXT: {{^    }}};

int selected_alternatives(Choice choice) {
  return match (choice) {
    case { int: 0 } => 1;
    case { .index<1>: double value } => static_cast<int>(value);
    case _ => 2;
  };
}

// CHECK-LABEL: int selected_alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return match (choice) {
// CHECK-NEXT: {{^        }}case { int: 0 } => 1;
// CHECK-NEXT: {{^        }}case { .index<1>: double value } => static_cast<int>(value);
// CHECK-NEXT: {{^        }}case _ => 2;
// CHECK-NEXT: {{^    }}};

int state_only_alternatives(Choice choice) {
  return match (choice) {
    case { .index<0> } => 0;
    case { .index<1> } => 1;
  };
}

// CHECK-LABEL: int state_only_alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return match (choice) {
// CHECK-NEXT: {{^        }}case { .index<0> } => 0;
// CHECK-NEXT: {{^        }}case { .index<1> } => 1;
// CHECK-NEXT: {{^    }}};

int constrained_alternative(Choice choice) {
  return match (choice) {
    case { Integral: auto value } => value;
    case { _ } => 0;
  };
}

// CHECK-LABEL: int constrained_alternative(Choice choice) {
// CHECK-NEXT: {{^    }}return match (choice) {
// CHECK-NEXT: {{^        }}case { Integral: auto value } => value;
// CHECK-NEXT: {{^        }}case { _ } => 0;
// CHECK-NEXT: {{^    }}};

int pointer(int *value) {
  return match (value) {
    case { int& projected } => projected;
    case {} => -1;
  };
}

// CHECK-LABEL: int pointer(int *value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case { int &projected } => projected;
// CHECK-NEXT: {{^        }}case {} => -1;
// CHECK-NEXT: {{^    }}};

template<int... Values>
int packs(Pair pair) {
  return match (pair) {
    case [Values..., 0] => 1L;
    case auto [...elements] => (... + elements);
  };
}

// CHECK-LABEL: template <int ...Values> int packs(Pair pair) {
// CHECK-NEXT: {{^    }}return match (pair) {
// CHECK-NEXT: {{^        }}case [Values..., 0] => 1L;
// CHECK-NEXT: {{^        }}case auto [...elements] => (... + elements);
// CHECK-NEXT: {{^    }}};

int unnamed_binding_pack(Four value) {
  return match (value) {
    case auto [first, ..., last] => first + last;
  };
}

// CHECK-LABEL: int unnamed_binding_pack(Four value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case auto [first, ..., last] => first + last;
// CHECK-NEXT: {{^    }}};

int wildcard_pack(Four value) {
  return match (value) {
    case [auto&& first, ..., auto&& last] => first + last;
  };
}

// CHECK-LABEL: int wildcard_pack(Four value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case [auto &&first, ..., auto &&last] => first + last;
// CHECK-NEXT: {{^    }}};

struct EmptyDecomposition {};

int empty_decomposition(EmptyDecomposition value) {
  return match (value) {
    case [] => 42;
  };
}

// CHECK-LABEL: int empty_decomposition(EmptyDecomposition value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case [] => 42;
// CHECK-NEXT: {{^    }}};

template<int I>
constexpr int constexpr_selection() {
  return match constexpr (I) -> int {
    case 0 => 1;
    case _ => static_assert(false);
  };
}

// CHECK-LABEL: template <int I> constexpr int constexpr_selection() {
// CHECK-NEXT: {{^    }}return match constexpr (I) -> int {
// CHECK-NEXT: {{^        }}case 0 => 1;
// CHECK-NEXT: {{^        }}case _ => static_assert(false);
// CHECK-NEXT: {{^    }}};

template<class T>
int attributed_cases(T value) {
  return match (value) {
    [[likely]] case int integer => integer;
    [[unlikely]] case _ => 0;
  };
}

// CHECK-LABEL: template <class T> int attributed_cases(T value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        \[\[likely\]\] case int integer => integer;}}
// CHECK-NEXT: {{^        \[\[unlikely\]\] case _ => 0;}}
// CHECK-NEXT: {{^    }}};

int instantiate_attributed_cases = attributed_cases(1);

int potentially_returns();

int nonreturning_handler(int value) {
  return match (value) {
    case 0 => !return potentially_returns();
    case _ => 42;
  };
}

// CHECK-LABEL: int nonreturning_handler(int value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case 0 => not return potentially_returns();
// CHECK-NEXT: {{^        }}case _ => 42;
// CHECK-NEXT: {{^    }}};

enum class PreambleKind { first, second };

int match_preamble(PreambleKind kind) {
  return match (kind) {
    using Result = int;
    static_assert(sizeof(Result) == sizeof(int));
    case PreambleKind::first => Result{1};
    case PreambleKind::second => Result{2};
  };
}

// CHECK-LABEL: int match_preamble(PreambleKind kind) {
// CHECK-NEXT: {{^    }}return match (kind) {
// CHECK-NEXT: {{^        }}using Result = int;
// CHECK-NEXT: {{^        }}static_assert(sizeof(Result) == sizeof(int));
// CHECK-NEXT: {{^        }}case PreambleKind::first => Result{1};
// CHECK-NEXT: {{^        }}case PreambleKind::second => Result{2};
// CHECK-NEXT: {{^    }}};

int or_pattern(int value) {
  return match (value) {
    case 0 || 1 || 2 => 1;
    case _ => 0;
  };
}

// CHECK-LABEL: int or_pattern(int value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case 0 || 1 || 2 => 1;
// CHECK-NEXT: {{^        }}case _ => 0;
// CHECK-NEXT: {{^    }}};

int parenthesized_pattern(int value) {
  return match (value) {
    case ((0 || 1)) => 1;
    case (_) => 0;
  };
}

// CHECK-LABEL: int parenthesized_pattern(int value) {
// CHECK-NEXT: {{^    }}return match (value) {
// CHECK-NEXT: {{^        }}case ((0 || 1)) => 1;
// CHECK-NEXT: {{^        }}case (_) => 0;
// CHECK-NEXT: {{^    }}};

int multiple_subjects(int first, long second) {
  return match (first, static_cast<long&&>(second)) {
    case [int x, long y] => x + y;
  };
}

// CHECK-LABEL: int multiple_subjects(int first, long second) {
// CHECK-NEXT: {{^    }}return match (first, static_cast<long &&>(second)) {
// CHECK-NEXT: {{^        }}case [int x, long y] => x + y;
// CHECK-NEXT: {{^    }}};
