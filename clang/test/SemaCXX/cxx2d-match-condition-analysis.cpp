// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wall -Wextra -Wuninitialized -verify %s

void use(int);

void match_condition(int value) {
  if (value match case int copy)
    use(copy);
}

void case_condition(int value) {
  if (case int copy = value)
    use(copy);
}

void guarded_condition(int value) {
  if (value match case int copy
      if (int doubled = copy * 2; doubled > 0))
    use(copy + doubled);
}

void condition_variable(int value) {
  if (case int copy = value if (int adjusted = copy + 1))
    use(copy + adjusted);
}

void while_condition(int value) {
  while (value match case int copy if (copy-- > 0))
    use(copy);
}

void for_condition(int value) {
  for (; value match case int copy if (copy > 0); use(copy))
    use(copy);
}

void unused_condition_binding(int value) {
  if (value match case int copy) { // expected-warning {{unused variable 'copy'}}
  }
}

constexpr int consteval_condition() {
  if consteval {
    return 1;
  } else {
    return 0;
  }
}

static_assert(consteval_condition() == 1);
