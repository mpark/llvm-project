// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

void valid(int first, int second) {
  bool test = match(first, second, case [_, _]);
  bool structured = match(first, second, case auto&& [x, y]);
  bool wildcard = match(first, second, case _);
  bool comma_expression =
      match((static_cast<void>(first), second), case int);

  match (first, second) {
    case [_, _] =>;
  }

  match (first, second) {
    case auto&& [x, y] => (void)(x + y);
  }

  match (first, second) {
    case auto [x, y] => (void)(x + y);
  }

  match (first, second) {
    case ([0, _] || [_, 0]) =>;
    case [_, _] =>;
  }

  match (first, second) {
    case [0, _] || _ =>;
  }

  match (first, second) {
    case _ =>;
  }
}

template<auto Value>
struct Constant {};

template<auto& Value>
struct Reference {};

static constexpr int constant_first = 1;
static constexpr int constant_second = 2;

void constexpr_bindings() {
  match (1, 2) {
    case constexpr auto [x, y] => {
      Constant<x> first;
      Constant<y> second;
    }
  }

  match (constant_first, constant_second) {
    case constexpr auto [x, y] => {
      Constant<x> first;
      Constant<y> second;
      Reference<x> first_reference;
      Reference<y> second_reference;
    }
  }

  match (1, 2) {
    case [constexpr int x, constexpr int y] => {
      Constant<x> first;
      Constant<y> second;
    }
  }
}

void invalid(int first, int second) {
  match(first, second, case auto&& whole); // expected-error {{multiple match subjects require a decomposition pattern, structured binding declaration pattern, or wildcard}}

  match (first, second) {
    case auto&& whole =>; // expected-error {{multiple match subjects require a decomposition pattern, structured binding declaration pattern, or wildcard}}
  }

  match (first, second) {
    case [0, _] || 1 =>; // expected-error {{multiple match subjects require a decomposition pattern, structured binding declaration pattern, or wildcard}}
    case [_, _] =>;
  }
}

void malformed(int value) {
  match(, case _); // expected-error {{expected expression}}
  match(value, case); // expected-error {{expected expression}}
}
