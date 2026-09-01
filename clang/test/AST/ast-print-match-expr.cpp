// RUN: %clang_cc1 -std=c++2d -fpattern-matching -ast-print %s | FileCheck %s

namespace std {
template<class T> struct alternative_traits;

template<class Provider>
struct alternative_name {
  using provider = Provider;
  __SIZE_TYPE__ index;
  consteval alternative_name(__SIZE_TYPE__ value) : index(value) {}
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
  using AT = alternative_traits;
  static constexpr __SIZE_TYPE__ size = 2;

  struct names {
    static constexpr alternative_name<AT> integer = 0, real = 1;
  };

  static constexpr __SIZE_TYPE__ index(const Choice& value) noexcept {
    return value.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& value) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(value).integer);
    else
      return (static_cast<Self&&>(value).real);
  }
};

int select(Pair pair) {
  return pair match -> int {
    case [0, long second] if (second > 0) => second;
    case auto&& whole => 0;
  };
}

// CHECK-LABEL: int select(Pair pair) {
// CHECK-NEXT: {{^    }}return pair match -> int {
// CHECK-NEXT: {{^        }}case [0, long second] if (second > 0) => second;
// CHECK-NEXT: {{^        }}case auto &&whole => 0;
// CHECK-NEXT: {{^    }}};

bool tests(Pair pair) {
  bool direct = pair match case [int first, long second] if (first < second);
  if (case [int first, long second] = pair)
    return first < second;
  return direct;
}

// CHECK-LABEL: bool tests(Pair pair) {
// CHECK-NEXT: {{^    }}bool direct = pair match case [int first, long second] if (first < second);
// CHECK-NEXT: {{^    }}if (case [int first, long second] = pair)
// CHECK-NEXT: {{^        }}return first < second;
// CHECK-NEXT: {{^    }}return direct;

int alternatives(Choice choice) {
  return choice match {
    case { .integer: int value } => value;
    case { .real: double } => 0;
  };
}

// CHECK-LABEL: int alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return choice match {
// CHECK-NEXT: {{^        }}case { .integer: int value } => value;
// CHECK-NEXT: {{^        }}case { .real: double } => 0;
// CHECK-NEXT: {{^    }}};

int selected_alternatives(Choice choice) {
  return choice match {
    case { int: 0 } => 1;
    case { .[1]: double value } => static_cast<int>(value);
    case _ => 2;
  };
}

// CHECK-LABEL: int selected_alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return choice match {
// CHECK-NEXT: {{^        }}case { int: 0 } => 1;
// CHECK-NEXT: {{^        }}case { .[1]: double value } => static_cast<int>(value);
// CHECK-NEXT: {{^        }}case _ => 2;
// CHECK-NEXT: {{^    }}};

int state_only_alternatives(Choice choice) {
  return choice match {
    case { .[0] } => 0;
    case { .[1] } => 1;
  };
}

// CHECK-LABEL: int state_only_alternatives(Choice choice) {
// CHECK-NEXT: {{^    }}return choice match {
// CHECK-NEXT: {{^        }}case { .[0] } => 0;
// CHECK-NEXT: {{^        }}case { .[1] } => 1;
// CHECK-NEXT: {{^    }}};
int pointer(int *value) {
  return value match {
    case { int& projected } => projected;
    case {} => -1;
  };
}

// CHECK-LABEL: int pointer(int *value) {
// CHECK-NEXT: {{^    }}return value match {
// CHECK-NEXT: {{^        }}case { int &projected } => projected;
// CHECK-NEXT: {{^        }}case {} => -1;
// CHECK-NEXT: {{^    }}};

template<int... Values>
int packs(Pair pair) {
  return pair match {
    case [Values..., 0] => 1L;
    case auto [...elements] => (... + elements);
  };
}

// CHECK-LABEL: template <int ...Values> int packs(Pair pair) {
// CHECK-NEXT: {{^    }}return pair match {
// CHECK-NEXT: {{^        }}case [Values..., 0] => 1L;
// CHECK-NEXT: {{^        }}case auto [...elements] => (... + elements);
// CHECK-NEXT: {{^    }}};

int unnamed_binding_pack(Four value) {
  return value match {
    case auto [first, ..., last] => first + last;
  };
}

// CHECK-LABEL: int unnamed_binding_pack(Four value) {
// CHECK-NEXT: {{^    }}return value match {
// CHECK-NEXT: {{^        }}case auto [first, ..., last] => first + last;
// CHECK-NEXT: {{^    }}};

int wildcard_pack(Four value) {
  return value match {
    case [auto&& first, ..._, auto&& last] => first + last;
  };
}

// CHECK-LABEL: int wildcard_pack(Four value) {
// CHECK-NEXT: {{^    }}return value match {
// CHECK-NEXT: {{^        }}case [auto &&first, ..._, auto &&last] => first + last;
// CHECK-NEXT: {{^    }}};

struct EmptyDecomposition {};

int empty_decomposition(EmptyDecomposition value) {
  return value match {
    case [] => 42;
  };
}

// CHECK-LABEL: int empty_decomposition(EmptyDecomposition value) {
// CHECK-NEXT: {{^    }}return value match {
// CHECK-NEXT: {{^        }}case [] => 42;
// CHECK-NEXT: {{^    }}};

template<int I>
constexpr int constexpr_selection() {
  return I match constexpr -> int {
    case 0 => 1;
    case _ => static_assert(false);
  };
}

// CHECK-LABEL: template <int I> constexpr int constexpr_selection() {
// CHECK-NEXT: {{^    }}return I match constexpr -> int {
// CHECK-NEXT: {{^        }}case 0 => 1;
// CHECK-NEXT: {{^        }}case _ => static_assert(false);
// CHECK-NEXT: {{^    }}};

template<class T>
int attributed_cases(T value) {
  return value match {
    [[likely]] case int integer => integer;
    [[unlikely]] default => 0;
  };
}

// CHECK-LABEL: template <class T> int attributed_cases(T value) {
// CHECK-NEXT: {{^    }}return value match {
// CHECK-NEXT: {{^        \[\[likely\]\] case int integer => integer;}}
// CHECK-NEXT: {{^        \[\[unlikely\]\] default => 0;}}
// CHECK-NEXT: {{^    }}};

int instantiate_attributed_cases = attributed_cases(1);
