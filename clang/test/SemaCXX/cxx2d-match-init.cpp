// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -verify %s

constexpr int declaration_init_statement(int source) {
  return match (int value = source + 1; value) {
    case 1 => value + 20;
    case int copy => copy;
  };
}

static_assert(declaration_init_statement(0) == 21);
static_assert(declaration_init_statement(4) == 5);

constexpr int expression_init_statement(int source) {
  int value = 0;
  return match (value = source + 1; value) {
    case 1 => value + 30;
    case int copy => copy;
  };
}

static_assert(expression_init_statement(0) == 31);
static_assert(expression_init_statement(4) == 5);

constexpr int multiple_subjects_after_init(int source) {
  return match (int first = source + 1; first, source) {
    case [1, 0] => 40;
    case [int x, int y] => x + y;
  };
}

static_assert(multiple_subjects_after_init(0) == 40);
static_assert(multiple_subjects_after_init(4) == 9);

template <class T>
constexpr T dependent(T source) {
  return match (T value = source; value) {
    case T copy => copy;
  };
}

static_assert(dependent(42) == 42);

constexpr int alias_init_statement() {
  return match (using T = int; T{42}) {
    case int value => value;
  };
}

static_assert(alias_init_statement() == 42);

struct Tracker {
  int value;
  int *destructions;
  constexpr ~Tracker() { ++*destructions; }
};

constexpr bool init_lifetime() {
  int destructions = 0;
  int result = match (Tracker tracker{7, &destructions}; tracker.value) {
    case int value => value;
  };
  return result == 7 && destructions == 1;
}

static_assert(init_lifetime());

struct DefaultSubject {
  int value = 42;
  constexpr ~DefaultSubject() {}
};

constexpr const DefaultSubject &default_subject(
    const DefaultSubject &subject = DefaultSubject()) {
  return subject;
}

constexpr int default_subject_selection() {
  return match (default_subject()) {
    case [int value] => value;
  };
}

template <class T> constexpr int default_subject_product() {
  return match (default_subject(), T{1}) {
    case [[int value], int increment] => value + increment;
  };
}

constexpr bool default_subject_test() {
  return match(default_subject(), case [42]);
}

static_assert(default_subject_selection() == 42);
static_assert(default_subject_product<int>() == 43);
static_assert(default_subject_test());

void statement_forms(int source) {
  match (int value = source; value) {
    case int copy => (void)(value + copy);
  }

  (void)value; // expected-error {{use of undeclared identifier 'value'}}
}

void missing_subject() {
  match (int value = 0;) { // expected-error {{expected expression}}
    case _ => ;
  }
}

void condition_declaration_not_supported(int source) {
  match (int value = source) { // expected-error {{expected '(' for function-style cast or type construction}}
    case _ => ;
  }
}
