// RUN: %clang_cc1 -std=c++2d -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2d -fsyntax-only -verify \
// RUN:   -fexperimental-new-constant-interpreter %s

namespace std {
using size_t = decltype(sizeof(0));
template<class> struct tuple_size;
template<size_t, class> struct tuple_element;
}

constexpr int omit_middle() {
  int values[] = {1, 2, 3, 4};
  auto [first, ..., last] = values;
  return first + last;
}

static_assert(omit_middle() == 5);

constexpr bool omit_everything() {
  int values[] = {1, 2, 3, 4};
  auto [...] = values;
  return true;
}

static_assert(omit_everything());

struct FourMembers {
  int first;
  int second;
  int third;
  int fourth;
};

template<class T>
constexpr int omit_middle_dependent(T value) {
  auto [first, ..., last] = value;
  return first + last;
}

static_assert(omit_middle_dependent(FourMembers{1, 2, 3, 4}) == 5);

template<class T>
constexpr bool omit_everything_dependent(T value) {
  auto [...] = value;
  return true;
}

static_assert(omit_everything_dependent(FourMembers{}));

struct CountedTuple {
  int *projections;
};

template<> struct std::tuple_size<CountedTuple> {
  static constexpr size_t value = 3;
};

template<std::size_t I> struct std::tuple_element<I, CountedTuple> {
  using type = int;
};

template<std::size_t I>
constexpr int get(CountedTuple &&value) {
  ++*value.projections;
  return I;
}

constexpr bool initializes_unnamed_bindings() {
  int projections = 0;
  auto [...] = CountedTuple{&projections};
  return projections == 3;
}

static_assert(initializes_unnamed_bindings());

void too_many_named_bindings() {
  int values[1] = {};
  auto [first, ..., last] = values; // expected-error {{type 'int[1]' binds to 1 element, but 2 names were provided}}
}

void named_pack_still_requires_a_template() {
  int values[1] = {};
  auto [...pack] = values; // expected-error {{pack declaration outside of template}}
}
