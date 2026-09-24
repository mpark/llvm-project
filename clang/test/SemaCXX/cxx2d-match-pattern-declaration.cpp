// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wall -Wextra -Wno-misleading-indentation -verify %s

namespace std {
template <class T>
struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template <>
struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^double};
  static constexpr bool has_residual_states = false;

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.active;
  }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).integer);
    else
      return (static_cast<Self&&>(choice).real);
  }
};

struct ResidualChoice : Choice {};

template <>
struct std::alternative_traits<ResidualChoice>
    : std::alternative_traits<Choice> {
  static constexpr bool has_residual_states = true;
};

struct Pair {
  int first;
  int second;
};

struct Five {
  int first;
  int second;
  int third;
  int fourth;
  int fifth;
};

void use(int);
int classify(int&);
int classify(double&);
[[noreturn]] void stop();

int decomposition(Pair pair) {
  case [int first, int second] = pair;
  return first + second;
}

int refutable(Pair pair) {
  case [0, int value] = pair else return -1;
  return value;
}

int nested(Pair pair) {
  case [int first, int second] = pair;
  case [0, int value] = Pair{first, second} else return -1;
  return value;
}

int heterogeneous(Choice choice) {
  case { auto&& value } = choice;
  return classify(value);
}

int residual_with_else(ResidualChoice choice) {
  case { auto&& value } = choice else return -1;
  return classify(value);
}

int residual_without_else(ResidualChoice choice) {
  case { auto&& value } = choice;
  return classify(value);
}

int declaration_pack(Five value) {
  case [auto first, auto ...middle, auto last] = value;
  return first + (... + middle) + last;
}

void unnamed_pack(Five value) {
  case [...] = value;
}

void refutable_without_else(Pair pair) {
  case [0, int value] = pair; // expected-error {{refutable pattern declaration requires an 'else' statement}}
}

void redundant_else(Pair pair) {
  case [int first, int second] = pair else return; // expected-error {{match case is redundant}}
  use(first + second);
}

void fallthrough_else(Pair pair) {
  case [0, int value] = pair else {} // expected-error {{'else' statement of a pattern declaration must not complete normally}}
  use(value);
}

void conditional_fallthrough(Pair pair, bool condition) {
  case [0, int value] = pair else if (condition) return; // expected-error {{'else' statement of a pattern declaration must not complete normally}}
  use(value);
}

void nested_loop_fallthrough(Pair pair) {
  case [0, int value] = pair else { // expected-error {{'else' statement of a pattern declaration must not complete normally}}
    while (true)
      break;
  }
  use(value);
}

void nonreturning_else(Pair pair, bool condition) {
  case [0, int value] = pair else {
    if (condition)
      return;
    stop();
  }
  use(value);
}

void name_not_in_else(Pair pair) {
  case [0, int value] = pair else {
    use(value); // expected-error {{use of undeclared identifier 'value'}}
    return;
  }
}

void goto_out(Pair pair) {
  {
    case [0, int value] = pair else goto done;
    use(value);
  }
done:
  return;
}

void goto_into(Pair pair) {
  case [0, int value] = pair else goto success; // expected-error {{cannot jump from this goto statement to its label}}
  use(value); // expected-note {{jump enters a statement controlled by a pattern condition}}
success:
  return;
}

void loop_control(Pair pair) {
  for (;;) {
    case [0, int value] = pair else break;
    use(value);
    continue;
  }
}

template <class T>
void strict_viability(T value) {
  case [T& first, T& second] = value else return; // expected-error {{declaration pattern of type 'Pair &' is not an exact match for subject of type 'int'}}
  (void)first;
  (void)second;
}

void instantiate_strict_viability() {
  strict_viability(Pair{}); // expected-note {{in instantiation of function template specialization 'strict_viability<Pair>' requested here}}
}

template <class T>
void dependent_pattern(Pair pair) {
  case [T first, int second] = pair else return; // expected-error {{declaration pattern of type 'double' is not an exact match for subject of type 'int'}}
  (void)first;
  (void)second;
}

void instantiate_dependent_pattern() {
  dependent_pattern<int>({});
  dependent_pattern<double>({}); // expected-note {{in instantiation of function template specialization 'dependent_pattern<double>' requested here}}
}

template <int Expected>
void dependent_refutability(Pair pair) {
  case [Expected, int second] = pair; // expected-error {{refutable pattern declaration requires an 'else' statement}}
  use(second);
}

template void dependent_refutability<0>(Pair); // expected-note {{in instantiation of function template specialization 'dependent_refutability<0>' requested here}}

void direct_substatement(Pair pair, bool condition) {
  if (condition)
    case [int first, int second] = pair; // expected-error {{a pattern declaration must appear directly in a compound statement}}
}

void same_scope_redefinition(Pair pair) {
  int first = 0; // expected-note {{previous definition is here}}
  case [int first, int second] = pair; // expected-error {{redefinition of 'first'}}
  use(first);
  use(second);
}

void repeated_pattern_declaration(Pair pair) {
  case [int first, int second] = pair; // expected-note {{previous definition is here}}
  case [int first, int third] = pair; // expected-error {{redefinition of 'first'}}
  use(first);
  use(second);
  use(third);
}

void repeated_name_in_pattern(Pair pair) {
  case [int value, // expected-note {{previous definition is here}}
        int value] = pair; // expected-error {{redefinition of 'value'}}
}

void nested_scope_shadowing(Pair pair) {
  int first = 0;
  {
    case [int first, int second] = pair;
    use(first);
    use(second);
  }
  use(first);
}

void placeholder_names(Pair pair) {
  int _;
  case [int _, int _] = pair;
}

void tag_name(Pair pair) {
  struct first {};
  case [int first, int second] = pair;
  use(first);
  use(second);
}

void switch_lambda(int value) {
  switch (value) {
  case [] { return 1; }():
    return;
  }
}
