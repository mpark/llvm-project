// RUN: %clang_cc1 -std=c++2c -verify -fsyntax-only %s

template <unsigned N>
void decompose_array() {
  int arr[4] = {1, 2, 3, 5};
  auto [x, ... // #1
    rest, ...more_rest] = arr; // expected-error{{multiple packs in structured binding declaration}}
                               // expected-note@#1{{previous binding pack specified here}}

  auto [y...] = arr; // expected-error{{'...' must immediately precede declared identifier}}

  auto [...] = arr; // expected-warning {{unnamed structured binding packs are a C++2d extension}}
  auto [a, ..., b] = arr; // expected-warning {{unnamed structured binding packs are a C++2d extension}}
  auto [a1, ...] = arr; // expected-warning {{unnamed structured binding packs are a C++2d extension}}
  auto [..., b1] = arr; // expected-warning {{unnamed structured binding packs are a C++2d extension}}
}
