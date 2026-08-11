//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

#include <array>
#include <cassert>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

int match_pointer(int* pointer) {
  return pointer match {
    case { int& value } => value;
    case {} => -1;
  };
}

int match_optional(const std::optional<int>& value) {
  return value match {
    case { const int& number } => number;
    case {} => -1;
  };
}

int match_named_optional(const std::optional<int>& value) {
  return value match {
    case { .some: const int& number } => number;
    case { .none } => -2;
  };
}

int match_unique_ptr(const std::unique_ptr<int>& value) {
  return value match {
    case { int& number } => number;
    case {} => -3;
  };
}

int match_shared_ptr(const std::shared_ptr<int>& value) {
  return value match {
    case { .some: int& number } => number;
    case { .none } => -4;
  };
}

int match_nullable(const auto& value) {
  return value match {
    case { .some: const int& number } => number;
    case { .none } => -6;
  };
}

int match_rvalue_optional(std::optional<int>&& value) {
  return static_cast<std::optional<int>&&>(value) match {
    case { int&& number } => number;
    case {} => -1;
  };
}

int match_variant(const std::variant<int, double>& value) {
  return value match {
    case { const int& integer } => integer;
    case { const double& real } => static_cast<int>(real) + 10;
  };
}

int match_variant_type_patterns(const std::variant<int, double>& value) {
  return value match {
    case { const int& } => 12;
    case { const double& } => 13;
  };
}

int match_expected(const std::expected<int, long>& value) {
  return value match {
    case { .value: const int& result } => result;
    case { .error: const long& error } => static_cast<int>(error) + 20;
  };
}

int match_nullable_expected(const std::expected<int, long>& value) {
  return value match {
    case { .some: const int& result } => result;
    case { .none } => -5;
  };
}

int match_complete_nullable_view_after_primary(
    const std::expected<int, long>& value) {
  return value match {
    case { .value: 0 } => 20;
    case { .some: const int& result } => result;
    case { .none } => -7;
  };
}

int match_same_type_expected(const std::expected<int, int>& value) {
  return value match {
    case { .value: const int& result } => result;
    case { .error: const int& error } => error + 30;
  };
}

int match_void_expected(const std::expected<void, std::string>& value) {
  return value match {
    case { void } => 40;
    case { std::string } => 41;
  };
}

int match_nullable_void_expected(
    const std::expected<void, std::string>& value) {
  return value match {
    case { .some } => 42;
    case { .none } => 43;
  };
}

int classify(int&) { return 40; }
int classify(double&) { return 50; }

int match_generic_variant(std::variant<int, double>& value) {
  return value match {
    case { auto&& alternative } => classify(alternative);
  };
}

using ScalarOrPair =
    std::variant<int, std::tuple<int, int>, std::array<int, 2>>;

int match_scalar_or_pair(ScalarOrPair& value) {
  return value match {
    case { int scalar } => scalar;
    case { auto&& [x, y] } => x * 10 + y;
  };
}

template<class T>
int match_dependent_generic(T& value) {
  return value match {
    case { auto&& alternative } => classify(alternative);
  };
}

int classify_rvalue(int&&) { return 51; }
int classify_rvalue(double&&) { return 52; }

int match_generic_rvalue(std::variant<int, double>&& value) {
  return static_cast<std::variant<int, double>&&>(value) match {
    case { auto&& alternative } =>
        classify_rvalue(static_cast<decltype(alternative)&&>(alternative));
  };
}

int match_repeated_variant(const std::variant<int, int>& value) {
  return value match {
    case { const int& alternative } => alternative;
  };
}

int match_repeated_type(const std::variant<int, int>& value) {
  return value match {
    case { const int& } => 75;
  };
}

struct Shape {
  virtual ~Shape() = default;
};

struct Circle : Shape {};

int match_projected_downcast(const std::variant<Shape*, int>& value) {
  return value match {
    case { Circle* } => 76;
    case { _ } => 77;
  };
}

int match_repeated_value(const std::variant<int, int>& value) {
  return value match {
    case { 0 } => 100;
    case { const int& alternative } => alternative;
  };
}

int match_numeric_zero(const std::variant<int, double>& value) {
  return value match {
    case { 0 } => 80;
    case { _ } => 81;
  };
}

template<class T>
int match_dependent_zero(T value) {
  return value match {
    case 0 => 84;
    case _ => 85;
  };
}

int match_partially_viable_zero(const std::variant<int, std::string>& value) {
  return value match {
    case { 0 } => 82;
    case { _ } => 83;
  };
}

template<class T, class U>
int match_dependent_typed_variant(const std::variant<T, U>& value) {
  return value match {
    case { char c } => static_cast<int>(c);
    case { int i } => i;
    case { _ } => -1;
  };
}

struct TwoVariants {
  std::variant<int, double> first;
  std::variant<char, long> second;
};

int combine(int&, char&) { return 60; }
int combine(int&, long&) { return 61; }
int combine(double&, char&) { return 62; }
int combine(double&, long&) { return 63; }

int match_nested_variants(TwoVariants& value) {
  return value match {
    case [{ auto&& first }, { auto&& second }] => combine(first, second);
  };
}

int test_nested_variant(std::pair<std::variant<int, double>, int>& value) {
  if (value match case [{ auto&& alternative }, _])
    return classify(alternative);
  return -1;
}

bool accepts_guard(int& value) { return value == 20; }
bool accepts_guard(double& value) { return value == 2.5; }

int test_nested_variant_guard(
    std::pair<std::variant<int, double>, int>& value) {
  if (value match case [{ auto&& alternative }, _]
      if (accepts_guard(alternative)))
    return classify(alternative);
  else
    return -1;
}

int test_two_variants(TwoVariants& value) {
  if (value match case [{ auto&& first }, { auto&& second }])
    return combine(first, second);
  return -1;
}

bool test_direct_value(std::variant<int, double>& value) {
  return value match case { 0 };
}

int subject_evaluations;

std::pair<std::variant<int, double>, int>&
evaluate_subject_once(std::pair<std::variant<int, double>, int>& value) {
  ++subject_evaluations;
  return value;
}

bool test_subject_evaluated_once(
    std::pair<std::variant<int, double>, int>& value) {
  subject_evaluations = 0;
  bool result = evaluate_subject_once(value) match case [{ _ }, _];
  return result && subject_evaluations == 1;
}

int classify_nested(int&) { return 70; }
int classify_nested(double&) { return 71; }
int classify_nested(char&) { return 72; }
int classify_nested(long&) { return 73; }
int classify_nested(bool&) { return 74; }

template<class T>
int match_uneven_nested_variants(T& value) {
  return value match {
    case { { auto&& alternative } } => classify_nested(alternative);
  };
}

template<class T>
int match_repeated_nested_variants(T& value) {
  return value match {
    case { { auto&& alternative } } => classify_nested(alternative);
  };
}

struct PrvalueAlternative {
  int* destructions;
};

struct PrvalueProjection {
  const PrvalueProjection* self;
  int* destructions;

  explicit PrvalueProjection(int* destructions)
      : self(this), destructions(destructions) {}
  PrvalueProjection(const PrvalueProjection&) = delete;
  PrvalueProjection(PrvalueProjection&&) = delete;
  ~PrvalueProjection() { ++*destructions; }
};

template<>
struct std::alternative_traits<PrvalueAlternative> {
  static constexpr std::size_t size = 1;
  static constexpr bool is_exhaustive = true;

  static constexpr std::size_t index(const PrvalueAlternative&) noexcept {
    return 0;
  }

  template<std::size_t I, class Self>
    requires(I == 0)
  static PrvalueProjection get(Self&& self) {
    return PrvalueProjection(self.destructions);
  }
};

void test_prvalue_projection_initialization() {
  int destructions = 0;
  PrvalueAlternative alternative{&destructions};
  bool has_expected_identity = alternative match {
    case { PrvalueProjection value } => value.self == &value;
  };
  assert(has_expected_identity);
  assert(destructions == 1);
}

int main(int, char**) {
  int value = 42;
  assert(match_pointer(&value) == 42);
  assert(match_pointer(nullptr) == -1);
  assert(match_optional(7) == 7);
  assert(match_optional(std::nullopt) == -1);
  assert(match_named_optional(7) == 7);
  assert(match_named_optional(std::nullopt) == -2);
  assert(match_unique_ptr(std::make_unique<int>(8)) == 8);
  assert(match_unique_ptr(nullptr) == -3);
  assert(match_shared_ptr(std::make_shared<int>(9)) == 9);
  assert(match_shared_ptr(nullptr) == -4);
  assert(match_nullable(&value) == 42);
  assert(match_nullable(static_cast<int*>(nullptr)) == -6);
  assert(match_nullable(std::optional<int>(10)) == 10);
  assert(match_nullable(std::optional<int>()) == -6);
  assert(match_nullable(std::expected<int, long>(11)) == 11);
  assert(match_nullable(std::expected<int, long>(std::unexpected(1L))) == -6);
  assert(match_nullable(std::make_unique<int>(12)) == 12);
  assert(match_nullable(std::unique_ptr<int>()) == -6);
  assert(match_nullable(std::make_shared<int>(13)) == 13);
  assert(match_nullable(std::shared_ptr<int>()) == -6);
  assert(match_rvalue_optional(std::optional<int>(8)) == 8);
  assert(match_variant(9) == 9);
  assert(match_variant(2.5) == 12);
  assert(match_variant_type_patterns(9) == 12);
  assert(match_variant_type_patterns(2.5) == 13);
  assert(match_dependent_zero(0) == 84);
  assert(match_dependent_zero(1) == 85);
  assert(match_dependent_zero(0.0) == 84);
  assert(match_dependent_zero(1.0) == 85);
  assert(match_expected(10) == 10);
  assert(match_expected(std::unexpected(3L)) == 23);
  assert(match_nullable_expected(10) == 10);
  assert(match_nullable_expected(std::unexpected(3L)) == -5);
  assert(match_complete_nullable_view_after_primary(0) == 20);
  assert(match_complete_nullable_view_after_primary(10) == 10);
  assert(match_complete_nullable_view_after_primary(std::unexpected(3L)) ==
         -7);
  assert(match_same_type_expected(11) == 11);
  assert(match_same_type_expected(std::unexpected(4)) == 34);
  assert(match_void_expected({}) == 40);
  assert(match_void_expected(std::unexpected(std::string("error"))) == 41);
  assert(match_nullable_void_expected({}) == 42);
  assert(match_nullable_void_expected(
             std::unexpected(std::string("error"))) == 43);
  std::variant<int, double> generic = 12;
  assert(match_generic_variant(generic) == 40);
  generic = 1.5;
  assert(match_generic_variant(generic) == 50);
  ScalarOrPair scalar_or_pair = 6;
  assert(match_scalar_or_pair(scalar_or_pair) == 6);
  scalar_or_pair = std::tuple(2, 3);
  assert(match_scalar_or_pair(scalar_or_pair) == 23);
  scalar_or_pair = std::array{4, 5};
  assert(match_scalar_or_pair(scalar_or_pair) == 45);
  generic = 15;
  assert(match_dependent_generic(generic) == 40);
  generic = 2.5;
  assert(match_dependent_generic(generic) == 50);
  assert(match_generic_rvalue(std::variant<int, double>(16)) == 51);
  assert(match_generic_rvalue(std::variant<int, double>(3.5)) == 52);
  assert(match_repeated_variant(
             std::variant<int, int>(std::in_place_index<0>, 13)) == 13);
  assert(match_repeated_variant(
             std::variant<int, int>(std::in_place_index<1>, 14)) == 14);
  assert(match_repeated_type(
             std::variant<int, int>(std::in_place_index<0>, 13)) == 75);
  assert(match_repeated_type(
             std::variant<int, int>(std::in_place_index<1>, 14)) == 75);
  Circle circle;
  Shape shape;
  assert(match_projected_downcast(std::variant<Shape*, int>(&circle)) == 76);
  assert(match_projected_downcast(std::variant<Shape*, int>(&shape)) == 77);
  assert(match_projected_downcast(std::variant<Shape*, int>(0)) == 77);
  assert(match_repeated_value(
             std::variant<int, int>(std::in_place_index<0>, 0)) == 100);
  assert(match_repeated_value(
             std::variant<int, int>(std::in_place_index<1>, 0)) == 100);
  assert(match_repeated_value(
             std::variant<int, int>(std::in_place_index<1>, 17)) == 17);
  assert(match_numeric_zero(0) == 80);
  assert(match_numeric_zero(0.0) == 80);
  assert(match_numeric_zero(1) == 81);
  assert(match_numeric_zero(1.0) == 81);
  assert(match_partially_viable_zero(0) == 82);
  assert(match_partially_viable_zero(std::string("zero")) == 83);
  assert(match_dependent_typed_variant(
             std::variant<int, std::string>(42)) == 42);
  assert(match_dependent_typed_variant(
             std::variant<int, std::string>(std::string("other"))) == -1);
  assert(match_dependent_typed_variant(
             std::variant<char, std::string>('a')) == 'a');
  TwoVariants nested{
      std::variant<int, double>(std::in_place_index<0>, 1),
      std::variant<char, long>(std::in_place_index<0>, 'a')};
  assert(match_nested_variants(nested) == 60);
  nested.second.emplace<1>(2L);
  assert(match_nested_variants(nested) == 61);
  nested.first.emplace<1>(3.0);
  assert(match_nested_variants(nested) == 63);
  nested.second.emplace<0>('b');
  assert(match_nested_variants(nested) == 62);
  std::pair<std::variant<int, double>, int> direct{{20}, 0};
  assert(test_nested_variant(direct) == 40);
  assert(test_nested_variant_guard(direct) == 40);
  direct.first = 2.5;
  assert(test_nested_variant(direct) == 50);
  assert(test_nested_variant_guard(direct) == 50);
  direct.first = 3.5;
  assert(test_nested_variant_guard(direct) == -1);
  assert(test_subject_evaluated_once(direct));
  assert(test_two_variants(nested) == 62);
  std::variant<int, double> direct_value = 0;
  assert(test_direct_value(direct_value));
  direct_value = 0.0;
  assert(test_direct_value(direct_value));
  direct_value = 1;
  assert(!test_direct_value(direct_value));
  using UnevenVariants =
      std::variant<std::variant<int, double>, std::variant<char, long, bool>>;
  UnevenVariants uneven(std::in_place_index<0>, std::in_place_index<0>, 1);
  assert(match_uneven_nested_variants(uneven) == 70);
  std::get<0>(uneven).emplace<1>(2.0);
  assert(match_uneven_nested_variants(uneven) == 71);
  uneven.emplace<1>(std::in_place_index<0>, 'a');
  assert(match_uneven_nested_variants(uneven) == 72);
  std::get<1>(uneven).emplace<1>(3L);
  assert(match_uneven_nested_variants(uneven) == 73);
  std::get<1>(uneven).emplace<2>(true);
  assert(match_uneven_nested_variants(uneven) == 74);
  using RepeatedNestedVariants =
      std::variant<std::variant<int, double>, std::variant<int, double>>;
  RepeatedNestedVariants repeated(
      std::in_place_index<1>, std::in_place_index<1>, 4.0);
  assert(match_repeated_nested_variants(repeated) == 71);
  test_prvalue_projection_initialization();
  return 0;
}
