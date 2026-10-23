// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -verify=enabled %s
// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wno-redundant-match-cases -verify=disabled %s

// disabled-no-diagnostics

int redundant_case(bool value) {
  return match (value) {
    case _ => 0;
    case true => 1; // enabled-warning {{match case is redundant}}
  };
}

int redundant_or_alternative(bool value) {
  return match (value) {
    case false or false => 0; // enabled-warning {{or-pattern alternative is redundant}}
    case true => 1;
  };
}

struct Pair {
  int first;
  int second;
};

int redundant_pattern_declaration_else(Pair value) {
  case [int first, int second] = value else return 0; // enabled-warning {{match case is redundant}}
  return first + second;
}
