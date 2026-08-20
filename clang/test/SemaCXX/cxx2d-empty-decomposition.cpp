// RUN: %clang_cc1 -std=c++2d -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify=pre %s

struct Empty {};
struct One { int value; };

void empty_structured_binding() {
  auto [] = Empty{}; // pre-warning {{empty structured bindings are a C++2d extension}}
  auto [] = One{};   // expected-error {{type 'One' binds to 1 element, but no names were provided}} pre-error {{type 'One' binds to 1 element, but no names were provided}} pre-warning {{empty structured bindings are a C++2d extension}}
}
