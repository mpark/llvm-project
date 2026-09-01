// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wall -Wextra -Wuninitialized -verify %s

void use(int);

bool match_condition(int value) {
  return value match case int copy if (copy > 0);
}

void match_condition_does_not_inject_bindings(int value) {
  if (value match case [[maybe_unused]] int copy)
    use(copy); // expected-error {{use of undeclared identifier 'copy'}}
}

void case_condition(int value) {
  if (case int copy = value)
    use(copy);
}

void guarded_condition(int value) {
  bool positive = value match case int copy
      if (int doubled = copy * 2; doubled > 0);
  use(positive);
}

void rejected_pattern_init_statement(int value) {
  if (case int copy = value; copy > 0) { // expected-error {{a pattern condition cannot be used as an init-statement}}
  }
}

int declaration_init_with_pattern_condition(int value) {
  if (int offset = 1; case int copy = value)
    return copy + offset;
  return 0;
}

int same_name_subject = 42;

void same_name_subject_initializer() {
  if (case int same_name_subject = same_name_subject) { // expected-error {{pattern binding 'same_name_subject' cannot be used in its own subject initializer}}
  }

  if (case int same_name_subject = ::same_name_subject)
    use(same_name_subject);
}

void while_condition(int value) {
  while (case int& copy = value) {
    use(copy);
    break;
  }
}

void for_condition(int value) {
  for (; case int& copy = value; use(copy)) {
    use(copy);
    break;
  }
}

void unused_condition_binding(int value) {
  if (case int copy = value) { // expected-warning {{unused variable 'copy'}}
  }
}

bool irrefutable_match_condition(int value) {
  return value match case int;
}

constexpr int irrefutable_case_condition(int value) {
  if (case int copy = value)
    return copy;
}

static_assert(irrefutable_case_condition(4) == 4);

struct FiveElements {
  int first;
  int second;
  int third;
  int fourth;
  int fifth;
};

constexpr int irrefutable_binding_pack_condition(FiveElements value) {
  if (case auto&& [first, ...middle, last] = value)
    return first * 2 + (... + middle) + last * 3;
}

static_assert(irrefutable_binding_pack_condition({1, 2, 3, 4, 5}) == 26);

void rejected_case_condition_guard(int value) {
  if (case int copy = value if (copy > 0)) // expected-error {{a pattern condition cannot have a trailing guard}}
    use(0);
}

struct Pair {
  int first;
  int second;
};

namespace std {
template<class T>
struct alternative_traits;
}

struct Choice {
  unsigned active;
  int integer;
  double real;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
  using type = __type_pack_element<I, int, double>;

  static constexpr __SIZE_TYPE__ index(const Choice& value) noexcept {
    return value.active;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& value) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(value).integer);
    else
      return (static_cast<Self&&>(value).real);
  }
};

template<class V, class W>
constexpr int projected_condition_chain(V first, W second) {
  if (true && case { auto value } = first && value > 0 &&
      case { auto other } = second && other > value)
    return static_cast<int>(value + other);
  return -1;
}

static_assert(projected_condition_chain(Choice{0, 2, 0.0},
                                        Choice{1, 0, 3.0}) == 5);
static_assert(projected_condition_chain(Choice{0, 3, 0.0},
                                        Choice{1, 0, 2.0}) == -1);

struct ContextuallyBoolean {
  constexpr explicit operator bool() const { return true; }
};

constexpr bool operator&&(ContextuallyBoolean, bool) { return false; }

template<class T>
constexpr bool condition_chain_uses_builtin_and(T value) {
  if (value && case int projected = 42)
    return projected == 42;
  return false;
}

static_assert(condition_chain_uses_builtin_and(ContextuallyBoolean{}));

struct PrefixBoolean {
  bool value;
  constexpr explicit operator bool() const { return value; }
};

constexpr PrefixBoolean operator&&(PrefixBoolean, PrefixBoolean) {
  return {false};
}

constexpr bool condition_chain_preserves_prefix_overload() {
  if (PrefixBoolean{true} && PrefixBoolean{true} && case int projected = 42)
    return projected == 42;
  return false;
}

static_assert(!condition_chain_preserves_prefix_overload());

constexpr int condition_chain_short_circuits() {
  int evaluations = 0;
  auto next = [&](int value) {
    ++evaluations;
    return value;
  };

  if (next(0) && case int value = next(1))
    return value;
  if (case 0 = next(1) && next(2))
    return -1;
  return evaluations;
}

static_assert(condition_chain_short_circuits() == 2);

struct LifetimeProbe {
  int* alive;
  int* copies;
  bool is_copy;

  constexpr LifetimeProbe(int* alive, int* copies)
      : alive(alive), copies(copies), is_copy(false) {
    ++*alive;
  }
  constexpr LifetimeProbe(const LifetimeProbe& other)
      : alive(other.alive), copies(other.copies), is_copy(true) {
    ++*alive;
    ++*copies;
  }
  constexpr ~LifetimeProbe() {
    --*alive;
    if (is_copy)
      --*copies;
  }
};

constexpr int condition_chain_cleans_failed_bindings_before_else() {
  int alive = 0;
  int copies = 0;
  int observed = -1;
  if (case LifetimeProbe copy = LifetimeProbe(&alive, &copies) && false)
    observed = 100;
  else
    observed = alive * 10 + copies;
  return observed;
}

// The failed binding is gone, while the hidden subject holder extends the
// temporary subject through the entire if statement.
static_assert(condition_chain_cleans_failed_bindings_before_else() == 10);

constexpr int condition_chain_keeps_bindings_alive() {
  int alive = 0;
  int copies = 0;
  if (case LifetimeProbe copy = LifetimeProbe(&alive, &copies) &&
      case int observed_copies = copies && observed_copies == 1)
    return alive * 10 + copies;
  return -1;
}

static_assert(condition_chain_keeps_bindings_alive() == 21);

template<class V>
constexpr int dependent_constexpr_projected_condition() {
  constexpr V value{0, 42, 0.0};
  if constexpr (case { auto projected } = value && projected == 42)
    return static_cast<int>(projected);
  else
    return -1;
}

static_assert(dependent_constexpr_projected_condition<Choice>() == 42);

template<class V>
constexpr int dependent_constexpr_projected_condition_false() {
  constexpr V value{1, 42, 0.0};
  if constexpr (case { auto projected } = value && projected == 42) {
    static_assert(sizeof(V) == 0);
    return static_cast<int>(projected);
  } else {
    return -1;
  }
}

static_assert(dependent_constexpr_projected_condition_false<Choice>() == -1);

constexpr int projected_condition_loops(Choice value) {
  int result = 0;
  while (case { auto projected } = value && projected > 0 && result < 2)
    ++result;
  for (int count = 0;
       case { auto projected } = value && projected > 0 && count < 2;
       ++count)
    result += static_cast<int>(projected);
  return result;
}

static_assert(projected_condition_loops(Choice{0, 2, 0.0}) == 6);

int refutable_decomposition_condition(Pair value) {
  if (case [int first, 0] = value)
    return first;
}

template <class T>
constexpr int decomposition_arity(T value) {
  if constexpr (requires { value match case [_, _]; })
    return 2;
  else
    return 0;
}

static_assert(decomposition_arity(Pair{}) == 2);
static_assert(decomposition_arity(0) == 0);

template <class T>
void strict_dependent_case_condition(T value) {
  if (case [auto&& first, auto&& second] = value) { // expected-error {{cannot bind non-class, non-array type 'int'}}
    use(first);
    use(second);
  }
}

template void strict_dependent_case_condition(Pair);
template void strict_dependent_case_condition(int); // expected-note {{in instantiation of function template specialization 'strict_dependent_case_condition<int>' requested here}}

constexpr int direct_constexpr_condition() {
  if constexpr (case [int first, 0] = Pair{42, 0})
    return first;
  else
    return -1;
}

static_assert(direct_constexpr_condition() == 42);

template <int Second>
constexpr int dependent_constexpr_condition() {
  if constexpr (case [int first, 0] = Pair{42, Second}) {
    static_assert(Second == 0);
    return first;
  } else {
    static_assert(Second != 0);
    return -1;
  }
}

static_assert(dependent_constexpr_condition<0>() == 42);
static_assert(dependent_constexpr_condition<1>() == -1);

void nonconstant_constexpr_condition(int value) { // expected-note {{declared here}}
  if constexpr (case int copy = value) { // expected-error {{constexpr if condition is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}} expected-note {{in call to '<expression body>'}}
    use(copy);
  }
}

template <class T>
void strict_dependent_constexpr_condition(T value) {
  if constexpr (case [auto&& first, auto&& second] = value) { // expected-error {{cannot bind non-class, non-array type 'int'}}
    use(first);
    use(second);
  }
}

template void strict_dependent_constexpr_condition(Pair);
template void strict_dependent_constexpr_condition(int); // expected-note {{in instantiation of function template specialization 'strict_dependent_constexpr_condition<int>' requested here}}

int filtered_range() {
  Pair pairs[] = {{1, 0}, {2, 1}, {3, 0}};
  int sum = 0;
  for (case [int element, 0] : pairs)
    sum += element;
  return sum;
}

constexpr int irrefutable_range() {
  int values[] = {1, 2, 3};
  int sum = 0;
  for (case int value : values)
    sum += value;
  return sum;
}

static_assert(irrefutable_range() == 6);

constexpr int same_name_range_initializer() {
  int values[] = {1, 2, 3};
  int sum = 0;
  for (case int values : values)
    sum += values;
  return sum;
}

static_assert(same_name_range_initializer() == 6);

constexpr int condition_binding_lifetime(int value) {
  while (case int& copy = value) {
    ++copy;
    break;
  }
  for (int count = 0; case int& copy = value; ++copy) {
    if (++count == 2)
      break;
  }
  return value;
}

static_assert(condition_binding_lifetime(4) == 6);

constexpr int consteval_condition() {
  if consteval {
    return 1;
  } else {
    return 0;
  }
}

static_assert(consteval_condition() == 1);
