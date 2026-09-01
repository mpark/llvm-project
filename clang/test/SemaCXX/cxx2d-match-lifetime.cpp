// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wreturn-stack-address -Wno-unused-value -verify %s

struct Object {
  int value;
};

const Object &reference_to_copy(Object object) {
  return object match -> const Object & {
    case Object copy => copy; // expected-warning {{reference to stack memory associated with local variable 'copy' returned}}
  };
}

const int &reference_to_member(Object object) {
  return object match -> const int & {
    case Object copy => copy.value; // expected-warning {{reference to stack memory associated with local variable 'copy' returned}}
  };
}

const Object *pointer_to_copy(Object object) {
  return object match -> const Object * {
    case Object copy => &copy; // expected-warning {{address of stack memory associated with local variable 'copy' returned}}
  };
}

struct Pair {
  int first;
  int second;
};

const int &reference_to_binding(Pair pair) {
  return pair match -> const int & {
    case auto [first, second] => first; // expected-warning {{reference to stack memory associated with local variable 'first' returned}}
  };
}

const int *pointer_to_binding(Pair pair) {
  return pair match -> const int * {
    case auto [first, second] => &second; // expected-warning {{address of stack memory associated with local variable 'second' returned}}
  };
}

const int &reference_binding_decomposition(Pair &pair) {
  return pair match -> const int & {
    case auto &[first, second] => first;
  };
}

auto lambda_capture(Object object) {
  return object match {
    case Object copy => [&copy] { // expected-warning {{address of stack memory associated with local variable 'copy' returned}} expected-note {{captured by reference here}}
      return copy.value;
    };
  };
}

struct ReferenceMember {
  const Object &value;
};

ReferenceMember aggregate_reference(Object object) {
  return object match {
    case Object copy => ReferenceMember{copy}; // expected-warning {{address of stack memory associated with local variable 'copy' returned}}
  };
}

namespace std {
template <class T> struct alternative_traits;
}

struct Choice {
  unsigned active;
  Object alternatives[2];
};

template <> struct std::alternative_traits<Choice> {
  static constexpr unsigned size = 2;

  template <unsigned I>
  using type = Object;

  static constexpr unsigned index(const Choice &choice) noexcept {
    return choice.active;
  }

  template <unsigned I, class Self>
  static constexpr decltype(auto) get(Self &&self) {
    return static_cast<Self &&>(self).alternatives[I];
  }
};

const Object &specialized_case_lifetime(Choice choice) {
  return choice match -> const Object & {
    case { Object copy } => copy; // expected-warning {{reference to stack memory associated with local variable 'copy' returned}}
  };
}

auto ordinary_if_init_statement() {
  if (Object object{}; true)
    return [&object] { return object.value; }; // expected-warning {{address of stack memory associated with local variable 'object' returned}} expected-note {{captured by reference here}}
  __builtin_unreachable();
}

auto statement_handler(Object object) {
  object match {
    case Object copy => return [&copy] { return copy.value; }; // expected-warning {{address of stack memory associated with local variable 'copy' returned}} expected-note {{captured by reference here}}
  };
}

const Object *condition_init_lifetime(Object object) {
  if (Object init{}; object match case Object copy)
    return &init; // expected-warning {{address of stack memory associated with local variable 'init' returned}}
  __builtin_unreachable();
}

Object &reference_binding(Object &object) {
  return object match -> Object & {
    case Object &ref => ref;
  };
}

const Object &enclosing_local(bool condition) {
  Object local{};
  return condition match -> const Object & {
    case true => local; // expected-warning {{reference to stack memory associated with local variable 'local' returned}}
    case false => local; // expected-warning {{reference to stack memory associated with local variable 'local' returned}}
  };
}

template <class T>
const Object &dependent_reference_to_copy(T object) {
  return object match -> const Object & {
    case Object copy => copy; // expected-warning 2{{reference to stack memory associated with local variable 'copy' returned}}
  };
}

template const Object &dependent_reference_to_copy(Object); // expected-note {{in instantiation of function template specialization 'dependent_reference_to_copy<Object>' requested here}}

const int &guard_condition_declaration(Object object) {
  return object match -> const int & {
    case Object copy if (int value = copy.value) => value; // expected-warning {{reference to stack memory associated with local variable 'value' returned}}
    case _ => object.value; // expected-warning {{reference to stack memory associated with parameter 'object' returned}}
  };
}

const int &guard_init_statement(Object object) {
  return object match -> const int & {
    case Object copy if (int value = copy.value; value != 0) => value; // expected-warning {{reference to stack memory associated with local variable 'value' returned}}
    case _ => object.value; // expected-warning {{reference to stack memory associated with parameter 'object' returned}}
  };
}

const int *guard_condition_declaration_statement(Object object) {
  object match {
    case Object copy if (int value = copy.value) => return &value; // expected-warning {{address of stack memory associated with local variable 'value' returned}}
    case _ => return &object.value; // expected-warning {{address of stack memory associated with parameter 'object' returned}}
  };
}
