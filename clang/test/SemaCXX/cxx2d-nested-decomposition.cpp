// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2c -DPRE_CXX29 -fsyntax-only -verify=pre %s

namespace std {
using size_t = decltype(sizeof(0));
template<class> struct tuple_size;
template<size_t, class> struct tuple_element;
}

struct Pair {
  int first;
  int second;
};

struct Outer {
  Pair pair;
  int last;
};

constexpr int aggregate() {
  auto [[x, y], z] = Outer{{1, 2}, 3}; // pre-warning {{nested structured bindings are a C++2d extension}}
  return x + y + z;
}
static_assert(aggregate() == 6);

constexpr int array() {
  int values[2][2] = {{1, 2}, {3, 4}};
  auto [[a, b], [c, d]] = values;
  // pre-warning@-1 2 {{nested structured bindings are a C++2d extension}}
  return a + b + c + d;
}
static_assert(array() == 10);

struct Empty {};
struct WithEmpty {
  Empty empty;
  int value;
};

constexpr int empty() {
  auto [[], value] = WithEmpty{{}, 42}; // pre-warning {{nested structured bindings are a C++2d extension}} pre-warning {{empty structured bindings are a C++2d extension}}
  return value;
}
static_assert(empty() == 42);

template<class T>
constexpr int dependent(T value) {
  auto [[x, y], z] = value; // pre-warning {{nested structured bindings are a C++2d extension}}
  return x + y + z;
}
static_assert(dependent(Outer{{1, 2}, 3}) == 6);

struct Four {
  int a, b, c, d;
};
struct WithFour {
  Four values;
  int tail;
};

template<class T>
constexpr int nested_pack(T value) {
  auto [[first, ...middle, last], tail] = value; // pre-warning {{nested structured bindings are a C++2d extension}}
  return first + (... + middle) + last + tail;
}
static_assert(nested_pack(WithFour{{1, 2, 3, 4}, 5}) == 15);

struct Trace {
  int entries[4] = {};
  int next = 0;

  constexpr void record(int value) { entries[next++] = value; }
};

struct TracedInner {
  int first;
  int second;
  Trace *trace;
};

struct TracedOuter {
  TracedInner inner;
  int last;
  Trace *trace;
};

template<std::size_t I>
constexpr decltype(auto) get(TracedInner &&value) {
  value.trace->record(I + 1);
  if constexpr (I == 0)
    return static_cast<int &&>(value.first);
  else
    return static_cast<int &&>(value.second);
}

template<std::size_t I>
constexpr decltype(auto) get(TracedOuter &&value) {
  value.trace->record(I == 0 ? 0 : 3);
  if constexpr (I == 0)
    return static_cast<TracedInner &&>(value.inner);
  else
    return static_cast<int &&>(value.last);
}

template<> struct std::tuple_size<TracedInner> {
  static constexpr std::size_t value = 2;
};
template<std::size_t I> struct std::tuple_element<I, TracedInner> {
  using type = int;
};
template<> struct std::tuple_size<TracedOuter> {
  static constexpr std::size_t value = 2;
};
template<> struct std::tuple_element<0, TracedOuter> {
  using type = TracedInner;
};
template<> struct std::tuple_element<1, TracedOuter> {
  using type = int;
};

constexpr bool initializes_depth_first() {
  Trace trace;
  auto&& [[x, y], z] = TracedOuter{{1, 2, &trace}, 3, &trace}; // pre-warning {{nested structured bindings are a C++2d extension}}
  return x + y + z == 6 && trace.next == 4 && trace.entries[0] == 0 &&
         trace.entries[1] == 1 && trace.entries[2] == 2 &&
         trace.entries[3] == 3;
}
static_assert(initializes_depth_first());

struct InnerRvalue {
  int value;
};
struct OuterRvalue {
  InnerRvalue inner;
};

template<std::size_t I>
constexpr int &&get(InnerRvalue &&value) {
  static_assert(I == 0);
  return static_cast<int &&>(value.value);
}
template<std::size_t I>
int &get(InnerRvalue &) = delete;
template<std::size_t I>
constexpr InnerRvalue &&get(OuterRvalue &&value) {
  static_assert(I == 0);
  return static_cast<InnerRvalue &&>(value.inner);
}
template<std::size_t I>
InnerRvalue &get(OuterRvalue &) = delete;

template<> struct std::tuple_size<InnerRvalue> {
  static constexpr std::size_t value = 1;
};
template<> struct std::tuple_element<0, InnerRvalue> {
  using type = int;
};
template<> struct std::tuple_size<OuterRvalue> {
  static constexpr std::size_t value = 1;
};
template<> struct std::tuple_element<0, OuterRvalue> {
  using type = InnerRvalue;
};

constexpr int forwards_rvalue_projection() {
  auto&& [[value]] = OuterRvalue{{42}}; // pre-warning {{nested structured bindings are a C++2d extension}}
  return value;
}
static_assert(forwards_rvalue_projection() == 42);

struct InnerLvalue {
  int value;
};
template<std::size_t I>
constexpr int &get(InnerLvalue &value) {
  static_assert(I == 0);
  return value.value;
}
template<std::size_t I>
int &&get(InnerLvalue &&) = delete;
template<> struct std::tuple_size<InnerLvalue> {
  static constexpr std::size_t value = 1;
};
template<> struct std::tuple_element<0, InnerLvalue> {
  using type = int;
};

struct ReferenceOuter {
  InnerLvalue &inner;
};

constexpr int preserves_reference_member_category() {
  InnerLvalue inner{1};
  auto&& [[value]] = ReferenceOuter{inner}; // pre-warning {{nested structured bindings are a C++2d extension}}
  value = 7;
  return inner.value;
}
static_assert(preserves_reference_member_category() == 7);

struct MutableOuter {
  mutable Pair pair;
};

constexpr int preserves_mutable_member_type() {
  const MutableOuter outer{{1, 2}};
  auto&& [[x, y]] = static_cast<const MutableOuter&&>(outer); // pre-warning {{nested structured bindings are a C++2d extension}}
  x = 3;
  return x + y;
}
static_assert(preserves_mutable_member_type() == 5);

struct ConditionalOuter {
  Pair pair;
  bool value;
  constexpr explicit operator bool() const { return value; }
};

constexpr int condition() {
  if (auto [[x, y], value] = ConditionalOuter{{2, 3}, true}) // pre-warning {{nested structured bindings are a C++2d extension}}
    return x + y + value;
  return 0;
}
static_assert(condition() == 6);

constexpr int range_for() {
  Outer values[] = {{{1, 2}, 3}, {{4, 5}, 6}};
  int sum = 0;
  for (auto [[x, y], z] : values) // pre-warning {{nested structured bindings are a C++2d extension}}
    sum += x + y + z;
  return sum;
}
static_assert(range_for() == 21);

constexpr int capture() {
  auto [[x, y], z] = Outer{{1, 2}, 3}; // pre-warning {{nested structured bindings are a C++2d extension}}
  return [=] { return x + y + z; }();
}
static_assert(capture() == 6);

void attributes() {
  [[maybe_unused]] auto [[x, y], z] = Outer{{1, 2}, 3}; // pre-warning {{nested structured bindings are a C++2d extension}}
  auto [[a [[maybe_unused]], b], c] = Outer{{1, 2}, 3}; // pre-warning {{nested structured bindings are a C++2d extension}}
  auto [[clang::annotate_type("type")]] [[d, e], f] = Outer{{1, 2}, 3}; // pre-warning {{nested structured bindings are a C++2d extension}}
  auto [[clang::annotate_type("type")]] [g, h] = Pair{1, 2};
  auto&& [[clang::annotate_type("type")]] [i, j] = Pair{1, 2};
  auto *p = new auto [[clang::annotate_type("type")]](42);
  delete p;
}

#if !defined(PRE_CXX29)
constexpr int nested_declaration_pattern(Outer value) {
  return value match {
    case auto&& [[x, y], z] if (x < 0) => 0;
    case auto&& [[a, b], c] => a + b + c;
  };
}
static_assert(nested_declaration_pattern({{1, 2}, 3}) == 6);

constexpr int nested_pack_declaration_pattern(WithFour value) {
  return value match {
    case auto&& [[first, ...middle, last], tail] =>
        first + (... + middle) + last + tail;
  };
}
static_assert(nested_pack_declaration_pattern({{1, 2, 3, 4}, 5}) == 15);
#endif

void diagnostics() {
  auto [[a, b, c], d] = Outer{}; // expected-error {{type 'Pair' binds to 2 elements, but 3 names were provided}} pre-error {{type 'Pair' binds to 2 elements, but 3 names were provided}} pre-warning {{nested structured bindings are a C++2d extension}}
  auto [...[e, f], g] = Outer{}; // expected-error {{a nested structured binding cannot be a pack expansion}} pre-error {{a nested structured binding cannot be a pack expansion}} pre-warning {{nested structured bindings are a C++2d extension}}
  auto [[h, i], h] = Outer{}; // expected-error {{redefinition of 'h'}} expected-note {{previous definition is here}} pre-error {{redefinition of 'h'}} pre-note {{previous definition is here}} pre-warning {{nested structured bindings are a C++2d extension}}
}
