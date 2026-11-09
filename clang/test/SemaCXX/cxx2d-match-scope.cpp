// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fno-spell-checking -fsyntax-only %s -verify

namespace std {
template <class T> struct alternative_traits;

struct alternative_info {
  decltype(^^int) info;
  bool empty = false;

  consteval alternative_info(decltype(^^int) info, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct Choice {
  unsigned active;
  bool boolean;
  int integer;
};

template <> struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^bool, ^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Choice &choice) noexcept {
    return choice.active;
  }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self &&choice) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(choice).boolean);
    else
      return (static_cast<Self &&>(choice).integer);
  }
};

struct Single {
  int integer;
};

template <> struct std::alternative_traits<Single> {
  static constexpr alternative_info alternatives[] = {^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Single &) noexcept { return 0; }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self &&single) {
    return (static_cast<Self &&>(single).integer);
  }
};

namespace direct {
static_assert(match(42, case int value if (value == 42)));
static_assert(match(43, case int value if (value == 43)));
int leaked = value; // expected-error {{use of undeclared identifier 'value'}}

constexpr int value = 7;
static_assert(match(42, case int value if (value == 42)));
static_assert(value == 7);

static_assert(match(42, case int value if (
    match(43, case int value if (value == 43)) && value == 42)));
static_assert(match(42, case int value if ([value] { return value == 42; }())));
} // namespace direct

namespace projected {
static_assert(match(Choice{1, false, 42}, case { int value } if (value == 42)));
static_assert(match(Choice{1, false, 43}, case { const int &value } if (value == 43)));
static_assert(match(Choice{1, false, 42}, case { auto value } if (value == 42)));
static_assert(match(Single{42}, case { int value } if (value == 42)));
int leaked = value; // expected-error {{use of undeclared identifier 'value'}}
} // namespace projected

namespace selection {
constexpr int first = match (42) {
  case int value if (value == 0) => 0;
  case int value => value;
};
constexpr int second = match (43) {
  case int value => value;
};
static_assert(first == 42 && second == 43);

constexpr int projected = match (Choice{1, false, 42}) {
  case { bool value } => int(value);
  case { int value } => value;
};
static_assert(projected == 42);

constexpr int initialized = match (int value = 42; value) {
  case int bound if (int adjusted = bound + 1; adjusted == 43) => adjusted;
  case _ => 0;
};
static_assert(initialized == 43);
int leaked = adjusted; // expected-error {{use of undeclared identifier 'adjusted'}}
} // namespace selection

template <class T>
constexpr bool direct_test = match(T{42}, case auto value if (value == 42));
static_assert(direct_test<int> && direct_test<long>);

template <class T>
constexpr bool projected_test = match(T{1, false, 42}, case { int value } if (value == 42));
static_assert(projected_test<Choice>);

template <class T>
constexpr int select = match (T{1, false, 42}) {
  case { bool value } => int(value);
  case { int value } => value;
};
static_assert(select<Choice> == 42);

constexpr int decomposed_default(int result = match (Single{42}) {
  case auto [value] => value;
}) {
  return result;
}
static_assert(decomposed_default() == 42);

struct Member {
  int source = 42;
  bool direct = match(source, case int value if (value == source));
  bool projected = match(Choice{1, false, source}, case { int value } if (value == source));
  int decomposed = match (Single{source}) {
    case auto [value] => value;
  };
  bool decomposed_test = match(Single{source}, case auto [value] if (value == source));
  static constexpr bool constant = match(42, case int value if (value == 42));
};
constexpr Member member;
static_assert(member.direct && member.projected && Member::constant);
static_assert(member.decomposed == 42 && member.decomposed_test);

constexpr bool function_scope() {
  int value = 7;
  if (!match(42, case int value if (value == 42)))
    return false;
  if (!match(Choice{1, false, 42}, case { int value } if (value == 42)))
    return false;
  return value == 7;
}
static_assert(function_scope());
