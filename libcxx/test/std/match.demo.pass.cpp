//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

// Examples from the "This Paper" column of P2688R5's comparison tables.

#include <cassert>
#include <concepts>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

int matching_integrals(int x) {
  return x match {
    0 => 0;
    1 => 1;
    _ => 2;
  };
}

int matching_strings(const std::string& s) {
  return s match {
    "foo" => 0;
    "bar" => 1;
    _ => 2;
  };
}

int matching_tuples(std::pair<int, int> p) {
  return p match {
    [0, 0] => 0;
    [0, let y] => 10 + y;
    [let x, 0] => 20 + x;
    let [x, y] => 30 + x + y;
  };
}

using Number = std::variant<std::int32_t, std::int64_t, float, double>;

int matching_variants(Number v) {
  return v match {
    std::int32_t: let i32 => 100 + i32;
    std::int64_t: let i64 => 200 + static_cast<int>(i64);
    float: let f => 300 + static_cast<int>(f);
    double: let d => 400 + static_cast<int>(d);
  };
}

int matching_variant_concepts(Number v) {
  return v match {
    std::integral: let i => 500 + static_cast<int>(i);
    std::floating_point: let f => 600 + static_cast<int>(f);
  };
}

struct Shape {
  virtual ~Shape() = default;
};
struct Circle : Shape {
  int radius;
};
struct Rectangle : Shape {
  int width, height;
};

int get_area(const Shape& shape) {
  // R5 omits the trailing return type, but its handlers deduce different
  // types. Its specified -> auto deduction therefore requires -> int here.
  return shape match -> int {
    Circle: let [r] => 3.14 * r * r;
    Rectangle: let [w, h] => w * h;
  };
}

struct Rgb {
  int r, g, b;
};
struct Hsv {
  int h, s, v;
};

using Color = std::variant<Rgb, Hsv>;

struct Quit {};
struct Move {
  int x, y;
};
struct Write {
  std::string s;
};
struct ChangeColor {
  Color c;
};

using Command = std::variant<Quit, Move, Write, ChangeColor>;

int matching_nested_structures(Command cmd) {
  return cmd match {
    Quit: _ => 0;
    Move: let [x, y] => 100 + x + y;
    Write: let [text] => 200 + static_cast<int>(text.size());
    ChangeColor: [Rgb: let [r, g, b]] => 300 + r + g + b;
    ChangeColor: [Hsv: let [h, s, v]] => 400 + h + s + v;
  };
}

int main(int, char**) {
  assert(matching_integrals(0) == 0);
  assert(matching_integrals(1) == 1);
  assert(matching_integrals(2) == 2);

  assert(matching_strings("foo") == 0);
  assert(matching_strings("bar") == 1);
  assert(matching_strings("other") == 2);

  assert(matching_tuples({0, 0}) == 0);
  assert(matching_tuples({0, 3}) == 13);
  assert(matching_tuples({4, 0}) == 24);
  assert(matching_tuples({4, 5}) == 39);

  assert(matching_variants(Number(std::in_place_type<std::int32_t>, 1)) == 101);
  assert(matching_variants(Number(std::in_place_type<std::int64_t>, 2)) == 202);
  assert(matching_variants(Number(std::in_place_type<float>, 3)) == 303);
  assert(matching_variants(Number(std::in_place_type<double>, 4)) == 404);

  assert(matching_variant_concepts(
             Number(std::in_place_type<std::int32_t>, 1)) == 501);
  assert(matching_variant_concepts(
             Number(std::in_place_type<std::int64_t>, 2)) == 502);
  assert(matching_variant_concepts(Number(std::in_place_type<float>, 3)) == 603);
  assert(matching_variant_concepts(Number(std::in_place_type<double>, 4)) == 604);

  Circle circle;
  circle.radius = 2;
  Rectangle rectangle;
  rectangle.width = 3;
  rectangle.height = 4;
  assert(get_area(circle) == 12);
  assert(get_area(rectangle) == 12);

  assert(matching_nested_structures(Quit{}) == 0);
  assert(matching_nested_structures(Move{1, 2}) == 103);
  assert(matching_nested_structures(Write{"abc"}) == 203);
  assert(matching_nested_structures(ChangeColor{Rgb{1, 2, 3}}) == 306);
  assert(matching_nested_structures(ChangeColor{Hsv{4, 5, 6}}) == 415);
}
