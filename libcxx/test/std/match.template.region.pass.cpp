//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <any>
#include <cassert>
#include <expected>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

constexpr int direct_placeholder(int source) {
  match (source) {
    case auto value => {
      if constexpr (requires { value.size(); })
        return static_cast<int>(value.size());
      else
        return value;
    }
  }
}

constexpr int direct_structured_binding(const std::pair<int, int>& source) {
  match (source) {
    case const auto& [first, second] => {
      if constexpr (std::is_same_v<decltype(first), const int>)
        return first + second;
      else
        static_assert(false);
    }
  }
}

constexpr int direct_decomposition(const std::pair<int, int>& source) {
  match (source) {
    case [const auto& first, const auto& second] => {
      if constexpr (std::is_same_v<decltype(first), const int&>)
        return first + second;
      else
        static_assert(false);
    }
  }
}

constexpr int direct_array_binding() {
  int source[] = {20, 22};
  return match (source) {
    case auto [first, second] => first + second;
  };
}

constexpr int direct_array_reference() {
  int source[] = {20, 22};
  match (source) {
    case auto&& whole => {
      static_assert(std::is_same_v<decltype(whole), int (&)[2]>);
      return whole[0] + whole[1];
    }
  }
}

constexpr int direct_declaration(int source) {
  case [auto value] = std::tuple{source};
  if constexpr (std::is_same_v<decltype(value), int>)
    return value;
  else
    static_assert(false);
}

constexpr int direct_reference(int& source) {
  match (source) {
    case decltype(auto) value => {
      static_assert(std::is_same_v<decltype(value), int&>);
      return ++value;
    }
  }
}

constexpr int direct_whole_variant(const std::variant<int, std::string>& source) {
  return match (source) {
    case const auto& whole => static_cast<int>(whole.index());
  };
}

template <class Product>
constexpr int generic_triple(Product source) {
  return match (source) {
    case [auto first, auto second, auto third] => first + second + third;
    case _ => -1;
  };
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wredundant-match-cases"
constexpr int covered_direct_placeholder(int source) {
  return match (source) {
    case int => 42;
    case auto value => static_cast<int>(value.size());
  };
}
#pragma clang diagnostic pop

constexpr int direct_condition(int source) {
  if (case auto value = source && value > 0) {
    if constexpr (std::is_same_v<decltype(value), int>)
      return value;
    else
      static_assert(false);
  }
  return -1;
}

constexpr int singleton_projection(std::optional<int> source) {
  match (source) {
    case { auto payload } => {
      if constexpr (std::is_same_v<decltype(payload), int>)
        return payload;
      else
        static_assert(false);
    }
    case {} => return -1;
  }
}

constexpr int named_projection(std::optional<int> source) {
  match (source) {
    case { .value: const auto& payload } => {
      if constexpr (std::is_same_v<decltype(payload), const int&>)
        return payload;
      else
        static_assert(false);
    }
    case {} => return -1;
  }
}

constexpr bool projected_requires(std::optional<int> source) {
  return match (source) {
    case { auto payload } if (requires { payload.no_such_member(); }) => true;
    case { .value: auto payload } if (requires { payload.no_such_member(); }) => true;
    case _ => false;
  };
}

constexpr int named_expected_projection(const std::expected<int, int>& source) {
  match (source) {
    case { .value: const auto& payload } => {
      if constexpr (std::is_same_v<decltype(payload), const int&>)
        return payload;
      else
        static_assert(false);
    }
    case { .error: const auto& payload } => {
      if constexpr (std::is_same_v<decltype(payload), const int&>)
        return payload + 100;
      else
        static_assert(false);
    }
  }
}

constexpr int or_binding_value(int value) { return value; }
constexpr int or_binding_value(const std::string& value) {
  return 100 + static_cast<int>(value.size());
}

constexpr int nested_or_binding(const std::tuple<int, std::string>& source) {
  return match (source) {
    case ([int payload, ""] or [0, const std::string& payload]) or [int payload, "x"] =>
      or_binding_value(payload);
    case _ => -1;
  };
}

template <class Product>
constexpr int deduced_or_binding(const Product& source) {
  return match (source) {
    case [auto payload, 0 or ""] or [0 or "", auto payload] => or_binding_value(payload);
    case _ => -1;
  };
}

template <class Product>
constexpr int guarded_deduced_or_binding(const Product& source) {
  return match (source) {
    case [auto payload, 0 or ""] or [0 or "", auto payload]
        if (or_binding_value(payload) != 0) => or_binding_value(payload);
    case _ => -1;
  };
}

constexpr int indexed_projection(const std::variant<int, int>& source) {
  match (source) {
    case { .index<0>: const auto& payload } => {
      if constexpr (std::is_same_v<decltype(payload), const int&>)
        return payload;
      else
        static_assert(false);
    }
    case { .index<1>: const int& payload } => return payload + 100;
    case _ => return -1;
  }
}

int open_type_projection(const std::any& source) {
  match (source) {
    case { int: auto payload } => {
      if constexpr (std::is_same_v<decltype(payload), int>)
        return payload;
      else
        static_assert(false);
    }
    case _ => return -1;
  }
}

struct QualifiedChoice {
  unsigned active;
  int first;
  const int second;
};

template <>
struct std::alternative_traits<QualifiedChoice> {
  static constexpr alternative_info alternatives[] = {^^int, ^^int};
  static constexpr bool has_residual_states = false;

  static constexpr unsigned index(const QualifiedChoice& source) noexcept {
    return source.active;
  }

  template <unsigned I>
  static constexpr decltype(auto) get(QualifiedChoice& source) {
    if constexpr (I == 0)
      return (source.first);
    else
      return (source.second);
  }
};

constexpr int qualified_type_selector(QualifiedChoice& source) {
  match (source) {
    case { int: auto&& payload } => {
      if constexpr (std::is_same_v<decltype(payload), int&>) {
        return ++payload;
      } else {
        static_assert(std::is_same_v<decltype(payload), const int&>);
        return payload + 100;
      }
    }
  }
}

constexpr int qualified_constraint_selector(QualifiedChoice& source) {
  match (source) {
    case { std::integral: auto&& payload } => {
      if constexpr (std::is_same_v<decltype(payload), int&>) {
        return ++payload;
      } else {
        static_assert(std::is_same_v<decltype(payload), const int&>);
        return payload + 100;
      }
    }
  }
}

int namespace_declaration_pack = match (std::pair{20, 22}) {
  case [auto&& ...elements] => (... + elements);
};

int namespace_structured_binding_pack = match (std::pair{20, 22}) {
  case auto [...elements] => (... + elements);
};

int per_alternative_static(const std::variant<int, int>& value) {
  match (value) {
    case { auto payload } => {
      static int calls = 0;
      (void)payload;
      return ++calls;
    }
    case _ => return -1;
  }
}

int per_alternative_reference_static(const std::variant<int, int>& value) {
  match (value) {
    case { const auto& payload } => {
      static int calls = 0;
      (void)payload;
      return ++calls;
    }
    case _ => return -1;
  }
}

int captured_lambda_value(const std::variant<int, std::string>& value) {
  match (value) {
    case { auto payload } => {
      return [payload] {
        if constexpr (std::is_same_v<decltype(payload), int>)
          return payload;
        else
          return static_cast<int>(payload.size());
      }();
    }
    case _ => return -1;
  }
}

int local_class_size(const std::variant<int, std::string>& value) {
  match (value) {
    case { auto payload } => {
      struct Local {
        decltype(payload) object;
        int size() const { return static_cast<int>(sizeof(object)); }
      };
      return Local{payload}.size();
    }
    case _ => return -1;
  }
}

constexpr int nested_generic_match(const std::variant<int, double>& value) {
  return match (value) {
    case { auto payload } => [payload]<class T>(T extra) {
      return match (payload) -> int {
        case 0 => extra;
        case auto copy => static_cast<int>(copy) + extra;
      };
    }(3);
    case _ => -1;
  };
}

struct Pair {
  int first;
  int second;
};

int concrete_or_static(Pair source) {
  match (source) {
    case [int payload, 0] or [0, int payload] => {
      static int initial = payload;
      static int calls = 0;
      return 100 * initial + 10 * ++calls + payload;
    }
    case _ => return -1;
  }
}

int generic_or_static(Pair source) {
  match (source) {
    case [auto payload, 0] or [0, auto payload] => {
      static int initial = payload;
      static int calls = 0;
      return 100 * initial + 10 * ++calls + payload;
    }
    case _ => return -1;
  }
}

int concrete_projection_static(const std::variant<int, int>& source) {
  match (source) {
    case { int payload } => {
      static int calls = 0;
      return 10 * ++calls + payload;
    }
    case _ => return -1;
  }
}

int concrete_projection_after_coverage(const std::variant<int, int, int>& source) {
  match (source) {
    case { .index<0>: int } => return 0;
    case { int payload } => {
      static int calls = 0;
      return 10 * ++calls + payload;
    }
    case _ => return -1;
  }
}

int concrete_or_condition_static(Pair source) {
  if (case [int payload, 0] or [0, int payload] = source) {
    static int calls = 0;
    return 10 * ++calls + payload;
  }
  return -1;
}

int concrete_or_guard_static(Pair source) {
  int calls = 0;
  bool selected = match(source, case [int payload, 0] or [0, int payload]
      if ([&] {
        static int count = 0;
        calls = ++count;
        return payload > 0;
      }()));
  return selected ? calls : -calls;
}

struct Positive {
  int value;
  constexpr explicit operator bool() const { return value > 0; }
};

constexpr int concrete_or_declaration_guard(Pair source) {
  return match (source) {
    case [int payload, 0] or [0, int payload]
        if (Positive guard{payload}) => guard.value;
    case _ => -1;
  };
}

constexpr bool concrete_or_test_declaration_guard(Pair source) {
  return match(source, case [int payload, 0] or [0, int payload]
      if (Positive guard{payload}));
}

constexpr int concrete_or_reference(Pair& source) {
  return match (source) {
    case [int& payload, 0] or [0, int& payload] if (payload > 0) => [&] {
      struct Local {
        int value;
        constexpr int get() const { return value; }
      };
      Local copy{payload};
      return copy.get() + ++payload;
    }();
    case _ => -1;
  };
}

int generic_unnamed_or_static(Pair source) {
  match (source) {
    case [auto, 0] or [0, auto] => {
      static int calls = 0;
      return ++calls;
    }
    case _ => return -1;
  }
}

template <class T>
int concrete_or_static_in_template(Pair source) {
  match (source) {
    case [int payload, 0] or [0, int payload] => {
      static int calls = 0;
      return 10 * ++calls + payload;
    }
    case _ => return -1;
  }
}

constexpr int concrete_or_condition(Pair& source) {
  if (case [int& payload, 0] or [0, int& payload] = source &&
      payload > 0) {
    int original = payload;
    return original + ++payload;
  }
  return -1;
}

struct Triple {
  int first;
  int second;
  int third;
};

constexpr int or_structured_binding_pack(const std::variant<Pair, Triple>& value) {
  return match (value) {
    case { Pair: const auto& [...elements] } or
         { Triple: const auto& [...elements] } => (... + elements);
    case _ => -1;
  };
}

constexpr int jump_out(const std::variant<int, double>& value) {
  match (value) {
    case { auto payload } => {
      if (payload < 0)
        goto negative;
      return static_cast<int>(payload);
    }
  }
negative:
  return -1;
}

constexpr int local_label(const std::variant<int, double>& value) {
  match (value) {
    case { auto payload } => {
      if (payload < 0)
        goto negative;
      return static_cast<int>(payload);
    negative:
      return -1;
    }
    case _ => return -2;
  }
}

struct EnclosingObject {
  int extra;

  constexpr int classify(const std::variant<int, double>& value) const {
    match (value) {
      case { auto payload } => return static_cast<int>(payload) + extra;
      case _ => return -1;
    }
  }
};

constexpr int singleton_condition_scopes(std::optional<int> value) {
  case { int& original } = value else return -1;
  int result = 0;
  if (case { int& payload } = value)
    result += payload;
  while (case { int& payload } = value && payload > 0)
    --payload;
  for (; case { int& payload } = value && payload < 2; ++payload)
    result += payload;
  return result + original;
}

constexpr bool test() {
  assert(direct_placeholder(42) == 42);
  assert(direct_structured_binding({20, 22}) == 42);
  assert(direct_decomposition({20, 22}) == 42);
  assert(direct_array_binding() == 42);
  assert(direct_array_reference() == 42);
  assert(direct_declaration(42) == 42);
  int direct_value = 41;
  assert(direct_reference(direct_value) == 42);
  assert(direct_value == 42);
  assert(direct_whole_variant(42) == 0);
  assert(direct_whole_variant(std::string("text")) == 1);
  assert(generic_triple(std::pair{20, 22}) == -1);
  assert(generic_triple(std::tuple{10, 20, 12}) == 42);
  assert(covered_direct_placeholder(1) == 42);
  assert(direct_condition(42) == 42);
  assert(direct_condition(-1) == -1);
  Pair first_pair{3, 0};
  Pair second_pair{0, 4};
  assert(concrete_or_declaration_guard({7, 0}) == 7);
  assert(concrete_or_declaration_guard({0, 9}) == 9);
  assert(concrete_or_declaration_guard({-2, 0}) == -1);
  assert(concrete_or_declaration_guard({0, -2}) == -1);
  assert(concrete_or_test_declaration_guard({7, 0}));
  assert(concrete_or_test_declaration_guard({0, 9}));
  assert(!concrete_or_test_declaration_guard({-2, 0}));
  assert(!concrete_or_test_declaration_guard({0, -2}));
  assert(concrete_or_reference(first_pair) == 7);
  assert(first_pair.first == 4);
  assert(concrete_or_reference(second_pair) == 9);
  assert(second_pair.second == 5);
  assert(concrete_or_condition(first_pair) == 9);
  assert(first_pair.first == 5);
  assert(concrete_or_condition(second_pair) == 11);
  assert(second_pair.second == 6);
  assert(singleton_projection(42) == 42);
  assert(singleton_projection(std::nullopt) == -1);
  assert(named_projection(42) == 42);
  assert(named_projection(std::nullopt) == -1);
  assert(!projected_requires(42));
  assert(!projected_requires(std::nullopt));
  assert(named_expected_projection(std::expected<int, int>(42)) == 42);
  assert(named_expected_projection(
             std::expected<int, int>(std::unexpected(42))) == 142);
  assert(nested_or_binding({7, ""}) == 7);
  assert(nested_or_binding({0, "abc"}) == 103);
  assert(nested_or_binding({7, "x"}) == 7);
  assert(nested_or_binding({7, "abc"}) == -1);
  assert(deduced_or_binding(std::tuple<int, int>{7, 0}) == 7);
  assert(deduced_or_binding(std::tuple<int, int>{0, 7}) == 7);
  assert(deduced_or_binding(std::tuple<int, int>{7, 9}) == -1);
  assert(deduced_or_binding(std::tuple<int, std::string>{7, ""}) == 7);
  assert(deduced_or_binding(std::tuple<int, std::string>{0, "abc"}) == 103);
  assert(deduced_or_binding(std::tuple<int, std::string>{7, "abc"}) == -1);
  assert(deduced_or_binding(std::tuple<int, std::string>{0, ""}) == 0);
  assert(guarded_deduced_or_binding(std::tuple<int, int>{7, 0}) == 7);
  assert(guarded_deduced_or_binding(std::tuple<int, int>{0, 7}) == 7);
  assert(guarded_deduced_or_binding(std::tuple<int, int>{0, 0}) == -1);
  assert(guarded_deduced_or_binding(std::tuple<int, std::string>{7, ""}) == 7);
  assert(guarded_deduced_or_binding(std::tuple<int, std::string>{0, "abc"}) == 103);
  assert(guarded_deduced_or_binding(std::tuple<int, std::string>{0, ""}) == -1);
  assert(indexed_projection(
             std::variant<int, int>(std::in_place_index<0>, 42)) == 42);
  assert(indexed_projection(
             std::variant<int, int>(std::in_place_index<1>, 42)) == 142);
  QualifiedChoice source{0, 41, 42};
  assert(qualified_type_selector(source) == 42);
  source.active = 1;
  assert(qualified_type_selector(source) == 142);
  source.active = 0;
  source.first = 41;
  assert(qualified_constraint_selector(source) == 42);
  source.active = 1;
  assert(qualified_constraint_selector(source) == 142);
  assert(nested_generic_match(42) == 45);
  assert(nested_generic_match(2.0) == 5);
  assert(nested_generic_match(0) == 3);
  assert(or_structured_binding_pack(Pair{1, 2}) == 3);
  assert(or_structured_binding_pack(Triple{3, 4, 5}) == 12);
  assert(jump_out(std::variant<int, double>(42)) == 42);
  assert(jump_out(std::variant<int, double>(2.0)) == 2);
  assert(local_label(std::variant<int, double>(42)) == 42);
  assert(local_label(std::variant<int, double>(2.0)) == 2);
  assert(EnclosingObject{3}.classify(std::variant<int, double>(42)) == 45);
  assert(EnclosingObject{3}.classify(std::variant<int, double>(2.0)) == 5);
  assert(singleton_condition_scopes(42) == 45);
  assert(singleton_condition_scopes(0) == 3);
  assert(singleton_condition_scopes(std::nullopt) == -1);
  return true;
}

int main(int, char**) {
  assert(open_type_projection(42) == 42);
  assert(open_type_projection(4.2) == -1);
  assert(namespace_declaration_pack == 42);
  assert(namespace_structured_binding_pack == 42);

  std::variant<int, int> first(std::in_place_index<0>, 1);
  std::variant<int, int> second(std::in_place_index<1>, 2);
  assert(per_alternative_static(first) == 1);
  assert(per_alternative_static(second) == 1);
  assert(per_alternative_static(first) == 2);
  assert(per_alternative_static(second) == 2);
  assert(per_alternative_reference_static(first) == 1);
  assert(per_alternative_reference_static(second) == 1);
  assert(per_alternative_reference_static(first) == 2);
  assert(per_alternative_reference_static(second) == 2);

  assert(concrete_or_static({0, 7}) == 717);
  assert(concrete_or_static({3, 0}) == 723);
  assert(concrete_or_static({0, 4}) == 734);
  assert(concrete_or_static_in_template<int>({0, 7}) == 17);
  assert(concrete_or_static_in_template<int>({3, 0}) == 23);
  assert(concrete_or_static_in_template<long>({0, 7}) == 17);
  assert(concrete_or_static_in_template<long>({3, 0}) == 23);
  assert(generic_or_static({0, 7}) == 717);
  assert(generic_or_static({3, 0}) == 313);
  assert(generic_or_static({0, 4}) == 724);
  assert(generic_or_static({5, 0}) == 325);
  assert(generic_unnamed_or_static({0, 7}) == 1);
  assert(generic_unnamed_or_static({3, 0}) == 1);
  assert(generic_unnamed_or_static({0, 4}) == 2);
  assert(generic_unnamed_or_static({5, 0}) == 2);
  assert(concrete_projection_static(first) == 11);
  assert(concrete_projection_static(second) == 22);
  std::variant<int, int, int> partially_covered(std::in_place_index<0>, 42);
  assert(concrete_projection_after_coverage(partially_covered) == 0);
  partially_covered.emplace<1>(1);
  assert(concrete_projection_after_coverage(partially_covered) == 11);
  partially_covered.emplace<2>(2);
  assert(concrete_projection_after_coverage(partially_covered) == 22);
  partially_covered.emplace<1>(3);
  assert(concrete_projection_after_coverage(partially_covered) == 33);
  assert(concrete_or_condition_static({0, 7}) == 17);
  assert(concrete_or_condition_static({3, 0}) == 23);
  assert(concrete_or_guard_static({0, 7}) == 1);
  assert(concrete_or_guard_static({3, 0}) == 2);

  assert(captured_lambda_value(std::variant<int, std::string>(42)) == 42);
  assert(captured_lambda_value(std::variant<int, std::string>("text")) == 4);
  assert(local_class_size(std::variant<int, std::string>(42)) == sizeof(int));
  assert(local_class_size(std::variant<int, std::string>("text")) == sizeof(std::string));

  assert(jump_out(std::variant<int, double>(-1.0)) == -1);
  assert(local_label(std::variant<int, double>(-1.0)) == -1);

  static_assert(test());
  assert(test());
  return 0;
}
