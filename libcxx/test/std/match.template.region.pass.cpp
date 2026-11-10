//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

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
  assert(namespace_declaration_pack == 42);
  assert(namespace_structured_binding_pack == 42);

  std::variant<int, int> first(std::in_place_index<0>, 1);
  std::variant<int, int> second(std::in_place_index<1>, 2);
  assert(per_alternative_static(first) == 1);
  assert(per_alternative_static(second) == 1);
  assert(per_alternative_static(first) == 2);
  assert(per_alternative_static(second) == 2);

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
