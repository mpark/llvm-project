//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// UNSUPPORTED: no-exceptions
// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>
#include <optional>
#include <tuple>
#include <variant>

struct TwoEmptyStates {
  unsigned active;
  int payload;
};

template <>
struct std::alternative_traits<TwoEmptyStates> {
  enum class state : unsigned { first, second, value };
  static constexpr alternative_info alternatives[] = {{{}, true}, {{}, true}, ^^int};
  static constexpr bool has_residual_states        = false;

  static constexpr state index(const TwoEmptyStates& subject) noexcept { return static_cast<state>(subject.active); }

  template <state I>
    requires(I == state::value)
  static constexpr const int& get(const TwoEmptyStates& subject) noexcept {
    return subject.payload;
  }
};

template <class T>
constexpr int empty_states(const T& subject) {
  return match(subject) {
    case { .first } => 0;
    case {} => 1;
    case { int payload } => payload;
  };
}

static_assert(empty_states(TwoEmptyStates{0, 42}) == 0);
static_assert(empty_states(TwoEmptyStates{1, 42}) == 1);
static_assert(empty_states(TwoEmptyStates{2, 42}) == 42);

struct Counters {
  int alive      = 0;
  int copies     = 0;
  int predicates = 0;
};

struct Tracked {
  Counters& counts;
  int value;

  Tracked(Counters& counts, int value) :
      counts(counts), value(value) { ++counts.alive; }
      Tracked(const Tracked& other) : counts(other.counts), value(other.value) {
        ++counts.alive;
        ++counts.copies;
      }
      ~Tracked() { --counts.alive; }
    };

    struct SeesCopy {
      bool operator()(const std::tuple<int&, Tracked&>& subject) const {
        Counters& counts = std::get<1>(subject).counts;
        ++counts.predicates;
        return counts.alive == 2 && std::get<1>(subject).value >= 0;
      }
      bool operator()(const Tracked& subject) const {
        ++subject.counts.predicates;
        return subject.counts.alive == 2;
      }
    };
    inline constexpr SeesCopy sees_copy;

    struct Nonnegative {
      bool operator()(int value) const { return value >= 0; }
    };
    inline constexpr Nonnegative nonnegative;

    inline constexpr auto throws_on_test = [](const Tracked& subject) -> bool {
      assert(subject.counts.alive == 2);
      ++subject.counts.predicates;
      throw 42;
    };

    void deferred_and_ordered_bindings() {
      Counters counts;
      Tracked subject(counts, 42);
      int tag     = 1;
      auto pair   = std::tie(tag, subject);
      auto select = [&] {
    return match (pair) {
      case [0, Tracked copy] and sees_copy => copy.value;
      case _ => -counts.alive;
    };
  };
  assert(select() == -1);
  assert(counts.alive == 1 && counts.copies == 0 && counts.predicates == 0);
  tag = 0;
  assert(select() == 42);
  assert(counts.alive == 1 && counts.copies == 1 && counts.predicates == 1);
  subject.value = -1;
  assert(select() == -1);
  assert(counts.alive == 1 && counts.copies == 2 && counts.predicates == 2);
}

void binding_cleanup_policies() {
  Counters counts;
  Tracked subject(counts, 42);
  int result = match (subject) {
    case Tracked copy if (false) => -1;
    case const Tracked& original => original.counts.alive;
  };
  assert(result == 1 && counts.alive == 1 && counts.copies == 1);

  auto observe = [&](bool matched) {
    assert(!matched && counts.alive == 2);
  };
  observe(match(subject, case Tracked copy if (false)));
  assert(counts.alive == 1 && counts.copies == 2);

  if (case Tracked copy = subject)
    assert(counts.alive == 2 && copy.value == 42);
  else
    assert(false);
  assert(counts.alive == 1 && counts.copies == 3);
}

void return_from_ordered_binding(Counters& counts) {
  Tracked subject(counts, 42);
  match (subject) {
    case Tracked copy and sees_copy => { return; }
    case _ => assert(false);
  }
}

void exceptional_binding_cleanup() {
  Counters counts;
  Tracked subject(counts, 42);
  auto expect_throw = [&](auto action) {
    try {
      action();
      assert(false);
    } catch (int value) {
      assert(value == 42 && counts.alive == 1);
    }
  };
  expect_throw([&] {
    match (subject) {
      case Tracked copy and throws_on_test => assert(false);
      case _ => assert(false);
    }
  });
  expect_throw([&] {
    (void)match(subject, case Tracked copy if (throws_on_test(copy)));
  });
  expect_throw([&] {
    if (case Tracked copy = subject && throws_on_test(copy))
      assert(false);
  });
  assert(counts.copies == 3 && counts.predicates == 3);
}

void guarded_candidates() {
  for (unsigned index : {0u, 1u}) {
    std::variant<int, int> subject(std::in_place_index<0>, 42);
    if (index == 1)
      subject.emplace<1>(42);
    int guards = 0;
    assert(!match(subject,
                  case { auto& value and nonnegative } or { auto& value }
                      if (++guards, value == 0)));
    assert(guards == 1);
    guards = 0;
    int result = match (subject) {
      case { auto& value and nonnegative } or { auto& value }
          if (++guards, value == 0) => -1;
      case _ => 1;
    };
    assert(result == 1 && guards == 1);
  }
}

void nested_payloads() {
  using Subject = std::optional<std::optional<int>>;
  Subject subject;
  assert(!match(subject, case {{ 0 or 1 or 2 }}));
  subject.emplace();
  assert(!match(subject, case {{ 0 or 1 or 2 }}));
  subject->emplace(1);
  assert(match(subject, case {{ 0 or 1 or 2 }}));
  **subject = 3;
  assert(!match(subject, case {{ 0 or 1 or 2 }}));
}

int main(int, char**) {
  deferred_and_ordered_bindings();
  binding_cleanup_policies();
  Counters counts;
  return_from_ordered_binding(counts);
  assert(counts.alive == 0 && counts.copies == 1 && counts.predicates == 1);
  exceptional_binding_cleanup();
  guarded_candidates();
  nested_payloads();
  assert(empty_states(TwoEmptyStates{0, 42}) == 0);
  assert(empty_states(TwoEmptyStates{1, 42}) == 1);
  assert(empty_states(TwoEmptyStates{2, 42}) == 42);
  return 0;
}
