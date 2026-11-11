//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17, c++20, c++23, c++26
// UNSUPPORTED: no-exceptions, no-rtti
// REQUIRES: clang

// ADDITIONAL_COMPILE_FLAGS: -fpattern-matching

// Examples from "The Proposal in Examples" in P2688.

#include <cassert>
#include <cmath>
#include <expected>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

std::vector<std::string> events;

void expect_events(std::initializer_list<std::string> expected) {
  assert(events == std::vector<std::string>(expected));
  events.clear();
}

// --- Value patterns ---------------------------------------------------------

enum class Coin { Penny, Nickel, Dime, Quarter };

int value_in_cents(Coin coin) {
  return match (coin) {
    case Coin::Penny => 1;
    //   ^^^^^^^^^^^ value pattern
    case Coin::Nickel => 5;
    case Coin::Dime => 10;
    case Coin::Quarter => 25;
  };
}

// --- Declaration patterns ---------------------------------------------------

int next_roll = 4;
int following_roll = 4;

int dice_roll() { return std::exchange(next_roll, following_roll); }

void add_fancy_hat() { events.push_back("add fancy hat"); }
void remove_fancy_hat() { events.push_back("remove fancy hat"); }
void move_player(int spaces) {
  events.push_back("move " + std::to_string(spaces) + " spaces");
}

void take_turn() {
  match (dice_roll()) {
    case 3 => add_fancy_hat();
    case 7 => remove_fancy_hat();
    case int roll => move_player(roll);
    //   ^^^^^^^^ declaration pattern
  }
}

// --- Wildcard patterns ------------------------------------------------------

void reroll() { events.push_back("reroll"); }

void reroll_unless_special() {
  match (dice_roll()) {
    case 3 => add_fancy_hat();
    case 7 => remove_fancy_hat();
    case _ => reroll();
    //   ^ wildcard pattern
  }
}

// --- Or-patterns ------------------------------------------------------------

void lose_turn() { events.push_back("lose turn"); }

void take_turn_with_or_pattern() {
  match (dice_roll()) {
    case 1 or 2 => lose_turn();
    //   ^^^^^^ or-pattern
    case 3 => add_fancy_hat();
    case 7 => remove_fancy_hat();
    case int roll => move_player(roll);
  }
}

// --- Decomposition patterns and guards --------------------------------------

void snake_eyes() { events.push_back("snake eyes"); }
void boxcars() { events.push_back("boxcars"); }
void play_doubles(int roll) {
  std::string value = std::to_string(roll);
  events.push_back("doubles: " + value + " + " + value);
}

std::pair<int, int> next_pair = {4, 4};

std::pair<int, int> roll_two_dice() { return next_pair; }

void take_turn_with_pair() {
  match (roll_two_dice()) {
    case [1, 1] => snake_eyes();
    //   ^^^^^^ decomposition pattern
    case [6, 6] => boxcars();
    case [int r1, int r2] if (r1 == r2) => {
      play_doubles(r1);
    }
    case [int r1, int r2] => {
      move_player(r1 + r2);
    }
  }
}

// --- Multiple subjects ------------------------------------------------------

void take_turn_with_two_subjects() {
  match (dice_roll(), dice_roll()) {
    case [1, 1] => snake_eyes();
    case [6, 6] => boxcars();
    case [int r1, int r2] if (r1 == r2) => {
      play_doubles(r1);
    }
    case [int r1, int r2] => {
      move_player(r1 + r2);
    }
  }
}

// --- Optional ---------------------------------------------------------------

std::optional<int> next_optional_roll = 5;

std::optional<int> try_dice_roll() { return next_optional_roll; }

void roll_again() { events.push_back("roll again"); }

void take_optional_turn() {
  match (try_dice_roll()) {
    case { int roll } => move_player(roll);
    //   ^^^^^^^^^^^^ alternative pattern
    case {} => roll_again();
  }
}

// --- Variant ----------------------------------------------------------------

struct File { std::string name; };

struct Directory { std::string name; };

using SearchResult = std::variant<File, Directory>;

void open_file(const File &file) {
  events.push_back("open file: " + file.name);
}

void open_directory(const Directory &directory) {
  events.push_back("open directory: " + directory.name);
}

void open(const SearchResult &result) {
  match (result) {
    case { const File &file } => open_file(file);
    case { const Directory &directory } => open_directory(directory);
  }
}

// --- Expected ---------------------------------------------------------------

std::expected<int, std::string> parse_port(std::string_view text) {
  if (text == "443") return 443;
  return std::unexpected("invalid port: " + std::string(text));
}

void connect(int port) {
  events.push_back("connect to port " + std::to_string(port));
}
void report(const std::string &message) {
  events.push_back("error: " + message);
}

void connect_from(std::string_view text) {
  match (parse_port(text)) {
    case { int port } => connect(port);
    case { const std::string &message } => report(message);
  }
}

// --- Named selectors --------------------------------------------------------

std::expected<std::string, std::string>
read_file(std::string_view path) {
  if (path == "config.txt")
    return std::expected<std::string, std::string>(std::in_place, "threads=8");
  return std::unexpected("cannot read " + std::string(path));
}

void process(const std::string &contents) {
  events.push_back("contents: " + contents);
}

void read_config(std::string_view path) {
  auto result = read_file(path);

  match (result) {
    case { .value: const auto &contents } => process(contents);
    //     ^^^^^^ named selector
    case { .error: const auto &message } => report(message);
  }
}

// --- Polymorphic matching ---------------------------------------------------

struct Shape { virtual ~Shape() = default; };

struct Circle : Shape {
  explicit Circle(double radius) : radius_(radius) {}
  double radius() const { return radius_; }

private:
  double radius_;
};

struct Rectangle : Shape {
  Rectangle(double width, double height) : width_(width), height_(height) {}
  double width() const { return width_; }
  double height() const { return height_; }

private:
  double width_;
  double height_;
};

double compute_area(const Shape &shape) {
  return match (shape) {
    case const Circle &c => 3.14 * c.radius() * c.radius();
    case const Rectangle &r => r.width() * r.height();
    case _ => throw std::invalid_argument("unknown shape");
  };
}

// --- Pattern tests and conditions -------------------------------------------

bool inspect_search_result(SearchResult result) {
  bool is_file = match(result, case { const File & });
  //             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ test a single pattern

  if (case { const Directory &directory } = result) {
    // names are available here
    open_directory(directory);
  }
  return is_file;
}

// --- Driver -----------------------------------------------------------------

int main() {
  assert(value_in_cents(Coin::Penny) == 1);
  assert(value_in_cents(Coin::Nickel) == 5);
  assert(value_in_cents(Coin::Dime) == 10);
  assert(value_in_cents(Coin::Quarter) == 25);

  next_roll = 4;
  take_turn();
  expect_events({"move 4 spaces"});
  next_roll = 3;
  take_turn();
  expect_events({"add fancy hat"});
  next_roll = 7;
  take_turn();
  expect_events({"remove fancy hat"});

  next_roll = 3;
  reroll_unless_special();
  expect_events({"add fancy hat"});
  next_roll = 7;
  reroll_unless_special();
  expect_events({"remove fancy hat"});
  next_roll = 4;
  reroll_unless_special();
  expect_events({"reroll"});

  next_roll = 1;
  take_turn_with_or_pattern();
  expect_events({"lose turn"});
  next_roll = 2;
  take_turn_with_or_pattern();
  expect_events({"lose turn"});
  next_roll = 3;
  take_turn_with_or_pattern();
  expect_events({"add fancy hat"});
  next_roll = 7;
  take_turn_with_or_pattern();
  expect_events({"remove fancy hat"});
  next_roll = 4;
  take_turn_with_or_pattern();
  expect_events({"move 4 spaces"});

  next_pair = {1, 1};
  take_turn_with_pair();
  expect_events({"snake eyes"});
  next_pair = {6, 6};
  take_turn_with_pair();
  expect_events({"boxcars"});
  next_pair = {4, 4};
  take_turn_with_pair();
  expect_events({"doubles: 4 + 4"});
  next_pair = {2, 5};
  take_turn_with_pair();
  expect_events({"move 7 spaces"});

  next_roll = following_roll = 1;
  take_turn_with_two_subjects();
  expect_events({"snake eyes"});
  next_roll = following_roll = 6;
  take_turn_with_two_subjects();
  expect_events({"boxcars"});
  next_roll = following_roll = 4;
  take_turn_with_two_subjects();
  expect_events({"doubles: 4 + 4"});
  next_roll = 2;
  following_roll = 5;
  take_turn_with_two_subjects();
  expect_events({"move 7 spaces"});

  next_optional_roll = 5;
  take_optional_turn();
  expect_events({"move 5 spaces"});
  next_optional_roll = 0;
  take_optional_turn();
  expect_events({"move 0 spaces"});
  next_optional_roll.reset();
  take_optional_turn();
  expect_events({"roll again"});

  open(SearchResult{File{"notes.txt"}});
  expect_events({"open file: notes.txt"});
  open(SearchResult{Directory{"include"}});
  expect_events({"open directory: include"});
  connect_from("443");
  expect_events({"connect to port 443"});
  connect_from("oops");
  expect_events({"error: invalid port: oops"});
  read_config("config.txt");
  expect_events({"contents: threads=8"});
  read_config("missing.txt");
  expect_events({"error: cannot read missing.txt"});

  Circle circle(2.0);
  Rectangle rectangle(3.0, 4.0);
  assert(std::abs(compute_area(circle) - 12.56) < 1e-12);
  assert(compute_area(rectangle) == 12.0);

  Shape unknown;
  try {
    (void)compute_area(unknown);
    assert(false);
  } catch (const std::invalid_argument& error) {
    assert(std::string_view(error.what()) == "unknown shape");
  }

  assert(inspect_search_result(SearchResult{File{"main.cpp"}}));
  expect_events({});
  assert(!inspect_search_result(SearchResult{Directory{"include"}}));
  expect_events({"open directory: include"});
  return 0;
}
