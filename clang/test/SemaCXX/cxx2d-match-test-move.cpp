// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only %s -verify
// expected-no-diagnostics

namespace std {
template <class T> struct alternative_traits;

struct alternative_info {
  decltype(^^int) info = {};
  bool empty = false;

  consteval alternative_info(decltype(^^int) info = {}, bool empty = false)
      : info(info), empty(empty) {}
};
} // namespace std

struct MoveOnly {
  int value;
  int *moves;
  int *live;

  constexpr MoveOnly(int value, int &moves, int &live)
      : value(value), moves(&moves), live(&live) {
    ++live;
  }

  MoveOnly(const MoveOnly &) = delete;

  constexpr MoveOnly(MoveOnly &&other)
      : value(other.value), moves(other.moves), live(other.live) {
    other.value = -1;
    ++*moves;
    ++*live;
  }

  constexpr ~MoveOnly() { --*live; }
};

constexpr bool direct_move(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = match(static_cast<MoveOnly &&>(value),
                      case MoveOnly owned if (pass && owned.value == 42));
  return result == pass && value.value == -1 && moves == 1 && live == 1;
}

template <class T>
constexpr bool dependent_test(T &&value, bool pass) {
  return match(static_cast<T &&>(value),
               case T owned if (pass && owned.value == 42));
}

constexpr bool dependent_move(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = dependent_test(static_cast<MoveOnly &&>(value), pass);
  return result == pass && value.value == -1 && moves == 1 && live == 1;
}

constexpr bool dependent_reference() {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = dependent_test(value, true);
  return result && value.value == 42 && moves == 0 && live == 1;
}

struct MovePair {
  MoveOnly first;
  int second;
};

constexpr bool structured_binding_move(bool pass) {
  int moves = 0;
  int live = 0;
  MovePair value{{42, moves, live}, 0};
  bool result = match(static_cast<MovePair &&>(value),
                      case auto [owned, second]
                          if (pass && owned.value == 42 && second == 0));
  return result == pass && value.first.value == -1 && moves == 1 && live == 1;
}

constexpr bool array_binding_move(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value[1] = {{42, moves, live}};
  bool result = match(static_cast<MoveOnly (&&)[1]>(value),
                      case auto [owned] if (pass && owned.value == 42));
  return result == pass && value[0].value == -1 && moves == 1 && live == 1;
}

struct Choice {
  unsigned active;
  MoveOnly object;
  int integer;
};

template <> struct std::alternative_traits<Choice> {
  static constexpr alternative_info alternatives[] = {^^MoveOnly, ^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const Choice &choice) noexcept {
    return choice.active;
  }

  template <__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self &&choice) {
    if constexpr (I == 0)
      return (static_cast<Self &&>(choice).object);
    else
      return (static_cast<Self &&>(choice).integer);
  }
};

constexpr bool valid(const MoveOnly &owned) { return owned.value == 42; }
constexpr bool valid(int owned) { return owned == 42; }

constexpr bool projected_test(Choice &&choice, bool pass) {
  return match(static_cast<Choice &&>(choice),
               case { auto owned } if (pass && valid(owned)));
}

template <class T>
constexpr bool dependent_projected_test(T &&choice, bool pass) {
  return match(static_cast<T &&>(choice),
               case { auto owned } if (pass && valid(owned)));
}

constexpr bool projected_move(bool pass, bool dependent) {
  int moves = 0;
  int live = 0;
  Choice choice{0, {42, moves, live}, 42};
  bool result = dependent
                    ? dependent_projected_test(static_cast<Choice &&>(choice),
                                               pass)
                    : projected_test(static_cast<Choice &&>(choice), pass);
  return result == pass && choice.object.value == -1 && moves == 1 && live == 1;
}

constexpr bool projected_scalar() {
  int moves = 0;
  int live = 0;
  Choice choice{1, {0, moves, live}, 42};
  bool result = projected_test(static_cast<Choice &&>(choice), true);
  return result && choice.object.value == 0 && moves == 0 && live == 1;
}

constexpr bool observe_alive(bool result, bool expected, int &live, int count) {
  return result == expected && live == count;
}

constexpr bool full_expression_move(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = observe_alive(
      match(static_cast<MoveOnly &&>(value),
            case MoveOnly owned if (pass && owned.value == 42)),
      pass, live, 2);
  return result && value.value == -1 && moves == 1 && live == 1;
}

constexpr bool full_expression_temporary(bool pass) {
  int moves = 0;
  int live = 0;
  bool result = observe_alive(
      match(MoveOnly(42, moves, live),
            case const MoveOnly &value if (pass && value.value == 42)),
      pass, live, 1);
  return result && moves == 0 && live == 0;
}

constexpr int with_test(int value, bool) { return value; }

constexpr bool nested_test(bool pass, bool product) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = product
                    ? observe_alive(
                          match(with_test(42, match(static_cast<MoveOnly &&>(value),
                                                    case MoveOnly owned if (pass))),
                                7, case [42, 7]),
                          true, live, 2)
                    : observe_alive(
                          match(with_test(42, match(static_cast<MoveOnly &&>(value),
                                                    case MoveOnly owned if (pass))),
                                case 42),
                          true, live, 2);
  return result && moves == 1 && live == 1;
}

template <class T>
constexpr bool dependent_nested_test(T &&value, bool pass, int &live) {
  return observe_alive(
      match(with_test(42, match(static_cast<T &&>(value),
                                case auto owned if (pass))),
            case 42),
      true, live, 2);
}

constexpr bool dependent_nested_move(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = dependent_nested_test(static_cast<MoveOnly &&>(value), pass, live);
  return result && moves == 1 && live == 1;
}

template <class T>
constexpr bool dependent_projected_alive(T &&choice, bool pass, int &live,
                                         int count) {
  return observe_alive(match(static_cast<T &&>(choice),
                            case { auto owned } if (pass && valid(owned))),
                       pass, live, count);
}

constexpr bool full_expression_projected(bool pass, unsigned state,
                                         bool dependent) {
  int moves = 0;
  int live = 0;
  Choice choice{state, {42, moves, live}, 42};
  int count = state == 0 ? 2 : 1;
  bool result = dependent
                    ? dependent_projected_alive(
                          static_cast<Choice &&>(choice), pass, live, count)
                    : observe_alive(
                          match(static_cast<Choice &&>(choice),
                                case { auto owned } if (pass && valid(owned))),
                          pass, live, count);
  return result && moves == (state == 0 ? 1 : 0) && live == 1;
}

constexpr bool full_expression_pattern_failure(bool matches) {
  int moves = 0;
  int live = 0;
  MovePair value{{42, moves, live}, matches ? 1 : 0};
  bool result = observe_alive(
      match(static_cast<MovePair &&>(value),
            case [MoveOnly owned, 1] if (owned.value == 42)),
      matches, live, matches ? 2 : 1);
  return result && moves == (matches ? 1 : 0) && live == 1;
}

constexpr bool full_expression_short_circuit(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  bool result = match(static_cast<MoveOnly &&>(value),
                      case MoveOnly owned if (pass && owned.value == 42)) &&
                live == 2;
  return result == pass && moves == 1 && live == 1;
}

constexpr bool skipped_test(bool execute) {
  int moves = 0;
  int live = 0;
  bool result = execute && match(MoveOnly(42, moves, live),
                                 case auto owned if (owned.value == 42));
  return result == execute && moves == (execute ? 1 : 0) && live == 0;
}

constexpr bool condition_ends_before_body(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  if (match(static_cast<MoveOnly &&>(value),
            case MoveOnly owned if (pass && owned.value == 42)))
    return pass && moves == 1 && live == 1;
  return !pass && moves == 1 && live == 1;
}

constexpr bool mixed_condition(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  if (case MoveOnly saved = static_cast<MoveOnly &&>(value) &&
      match(MoveOnly(42, moves, live),
            case auto owned if (pass && owned.value == 42)) &&
      live == 4)
    return pass && saved.value == 42 && moves == 2 && live == 2;
  return !pass && moves == 2 && live == 1;
}

constexpr bool interleaved_condition(bool pass) {
  int moves = 0;
  int live = 0;
  MoveOnly value(42, moves, live);
  if (match(MoveOnly(42, moves, live), case const MoveOnly &) &&
      case MoveOnly saved = static_cast<MoveOnly &&>(value) &&
      match(MoveOnly(42, moves, live),
            case auto owned if (pass && owned.value == 42)) &&
      case int number = 42 && live == 5)
    return pass && saved.value == number && moves == 2 && live == 2;
  return !pass && moves == 2 && live == 1;
}

constexpr bool mixed_while_condition() {
  int moves = 0;
  int live = 0;
  int iterations = 0;
  MoveOnly value(42, moves, live);
  while (case const MoveOnly &saved = value &&
         match(MoveOnly(42, moves, live),
               case auto owned if (iterations++ == 0)) &&
         live == 3) {
    if (saved.value != 42 || live != 1)
      return false;
  }
  return iterations == 2 && moves == 2 && live == 1;
}

constexpr bool mixed_for_condition() {
  int moves = 0;
  int live = 0;
  int iterations = 0;
  MoveOnly value(42, moves, live);
  for (; case const MoveOnly &saved = value &&
         match(MoveOnly(42, moves, live),
               case auto owned if (iterations == 0)) &&
         live == 3;
       iterations += live) {
    if (saved.value != 42 || live != 1)
      return false;
  }
  return iterations == 1 && moves == 2 && live == 1;
}

struct LifetimeTrace {
  int &next;
  int *order;
  int &count;
  int id;

  constexpr LifetimeTrace(int &next, int *order, int &count)
      : next(next), order(order), count(count), id(++next) {}
  constexpr LifetimeTrace(LifetimeTrace &&other)
      : next(other.next), order(other.order), count(other.count), id(++next) {}
  constexpr ~LifetimeTrace() { order[count++] = id; }
};

constexpr bool test_destruction_order() {
  int next = 0;
  int count = 0;
  int order[2] = {};
  bool result = match(LifetimeTrace(next, order, count), case auto owned);
  return result && count == 2 && order[0] == 2 && order[1] == 1;
}

constexpr bool nested_destruction_order() {
  int next = 0;
  int count = 0;
  int order[2] = {};
  bool result = match(with_test(42, match(LifetimeTrace(next, order, count),
                                        case auto owned)),
                      case 42);
  return result && count == 2 && order[0] == 2 && order[1] == 1;
}

static_assert(__is_same(decltype(match(0, case 0)), bool));
static_assert(direct_move(true));
static_assert(direct_move(false));
static_assert(dependent_move(true));
static_assert(dependent_move(false));
static_assert(dependent_reference());
static_assert(structured_binding_move(true));
static_assert(structured_binding_move(false));
static_assert(array_binding_move(true));
static_assert(array_binding_move(false));
static_assert(projected_move(true, false));
static_assert(projected_move(false, false));
static_assert(projected_move(true, true));
static_assert(projected_move(false, true));
static_assert(projected_scalar());
static_assert(full_expression_move(true));
static_assert(full_expression_move(false));
static_assert(full_expression_temporary(true));
static_assert(full_expression_temporary(false));
static_assert(nested_test(true, false));
static_assert(nested_test(false, false));
static_assert(nested_test(true, true));
static_assert(nested_test(false, true));
static_assert(dependent_nested_move(true));
static_assert(dependent_nested_move(false));
static_assert(full_expression_projected(true, 0, false));
static_assert(full_expression_projected(false, 0, false));
static_assert(full_expression_projected(true, 1, false));
static_assert(full_expression_projected(false, 1, false));
static_assert(full_expression_projected(true, 0, true));
static_assert(full_expression_projected(false, 0, true));
static_assert(full_expression_projected(true, 1, true));
static_assert(full_expression_projected(false, 1, true));
static_assert(full_expression_pattern_failure(true));
static_assert(full_expression_pattern_failure(false));
static_assert(full_expression_short_circuit(true));
static_assert(full_expression_short_circuit(false));
static_assert(skipped_test(true));
static_assert(skipped_test(false));
static_assert(condition_ends_before_body(true));
static_assert(condition_ends_before_body(false));
static_assert(mixed_condition(true));
static_assert(mixed_condition(false));
static_assert(interleaved_condition(true));
static_assert(interleaved_condition(false));
static_assert(mixed_while_condition());
static_assert(mixed_for_condition());
static_assert(test_destruction_order());
static_assert(nested_destruction_order());

namespace namespace_scope_temporaries {
struct Box {
  int value;
  constexpr Box(const int &value) : value(value) {}
};

struct ReferenceBox {
  const int &value;
  constexpr ReferenceBox(const int &value) : value(value) {}
};

struct MutableReferenceBox {
  int &value;
  constexpr MutableReferenceBox(int &&value) : value(value) { ++value; }
};

static_assert(match(Box{42}, case const Box &));
static_assert(match(Box{42}, case [42]));
static_assert(!match(Box{42}, case [0]));
static_assert(match(Box{42}, case const Box &accepted if (accepted.value == 42)));
static_assert(!match(Box{42}, case const Box &rejected if (rejected.value == 0)));

static_assert(match(ReferenceBox{42}, case [42]));
static_assert(match(MutableReferenceBox{41}, case [42]));

constexpr bool initialized_by_test = match(Box{42}, case [42]);
static_assert(initialized_by_test);

template <int Value>
constexpr bool dependent_test = match(Box{Value}, case [Value]);
static_assert(dependent_test<42>);
} // namespace namespace_scope_temporaries
