// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value %s -verify

void test_throw_does_not_contribute_to_type_deduction() {
  static_assert(__is_same(decltype(match (0) {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  }), int));
}

void test_throw_action() {
  static_assert(match (0) {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 0);
  static_assert(match (1) {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 1);
}

double potentially_returns();

auto test_nonreturning_handler_result(int value) {
  return match (value) {
    case 0 => not return potentially_returns();
    case _ => 42;
  };
}

static_assert(__is_same(decltype(test_nonreturning_handler_result(0)), int));

template<class T>
T dependent_potentially_returns(T);

template<class T>
auto test_dependent_nonreturning_handler(bool selected, T value) {
  return match (selected) {
    case true => not return dependent_potentially_returns(value);
    case false => 42;
  };
}

static_assert(__is_same(decltype(test_dependent_nonreturning_handler(false, 1.0)),
                        int));

constexpr int test_unselected_nonreturning_handler() {
  return match (false) -> int {
    case true => not return 1;
    case false => 42;
  };
}

static_assert(test_unselected_nonreturning_handler() == 42);

constexpr int test_guard_rejects_nonreturning_handler() {
  return match (0) -> int {
    case _ if (false) => not return 1;
    case _ => 42;
  };
}

static_assert(test_guard_rejects_nonreturning_handler() == 42);

namespace match_preamble {
enum class Kind { first, second };

namespace constants {
inline constexpr int one = 1;
inline constexpr int two = 2;
} // namespace constants

namespace nested {
inline constexpr int three = 3;
} // namespace nested

constexpr int select_kind(Kind kind) {
  return match (kind) {
    using enum Kind;
    using Result = int;
    static_assert(__is_same(Result, int));
    case first => Result{1};
    case second => Result{2};
  };
}

constexpr int select_integer(int value) {
  return match (value) {
    using constants::one;
    using namespace constants;
    namespace n = nested;
    case one => 10;
    case two => 20;
    case n::three => 30;
    case _ => 0;
  };
}

template<class T> struct traits;
template<> struct traits<int> { using type = int; };

template<class T>
constexpr int dependent(T value) {
  return match (value) {
    using U = typename traits<T>::type;
    static_assert(__is_same(U, T));
    case U copy => copy;
  };
}

template<class T>
constexpr int dependent_preamble_only() {
  return match (0) {
    static_assert(sizeof(T) != 0);
    case _ => 42;
  };
}

constexpr int generic_projection_with_preamble(const int* pointer) {
  return match (pointer) {
    using Result = int;
    case { const auto& value } => Result(value);
    case {} => -1;
  };
}

static_assert(select_kind(Kind::first) == 1);
static_assert(select_kind(Kind::second) == 2);
static_assert(select_integer(1) == 10);
static_assert(select_integer(2) == 20);
static_assert(select_integer(3) == 30);
static_assert(dependent(42) == 42);
static_assert(dependent_preamble_only<int>() == 42);
constexpr int projected_value = 42;
static_assert(generic_projection_with_preamble(&projected_value) == 42);
static_assert(generic_projection_with_preamble(nullptr) == -1);

void simple_declaration_is_not_a_preamble(int value) {
  match (value) {
    int local = 1; // expected-error {{expected 'case' before pattern}}
    case _ => local; // expected-error {{use of undeclared identifier 'local'}}
  }
}

void preamble_must_precede_cases(int value) {
  match (value) {
    case 0 => ;
    using Alias = int; // expected-error {{match preamble declaration must precede all cases}}
    case Alias copy => copy;
  }
}

void preamble_scope_ends_with_selection(int value) {
  match (value) {
    using Alias = int;
    case _ => Alias{};
  }
  Alias outside; // expected-error {{unknown type name 'Alias'}}
}
} // namespace match_preamble

constexpr int test_returning_nonreturning_handler() {
  return match (true) -> int {
    case true => not return 1;
    case false => 42;
  };
}

static_assert(test_returning_nonreturning_handler() == 1); // expected-error {{static assertion expression is not an integral constant expression}} expected-note {{in call to 'test_returning_nonreturning_handler()'}}
// expected-note@-6 {{control returned from a 'not return' match handler}}

constexpr void test_null_and_static_assert_handlers(bool value) {
  match (value) {
    case true => ;
    case false => static_assert(sizeof(int) >= 2);
  }
}

static_assert((test_null_and_static_assert_handlers(true), true));
static_assert((test_null_and_static_assert_handlers(false), true));

auto test_null_handler_result_mismatch(int value) {
  return match (value) {
    case 0 => ;
    case _ => 1; // expected-error {{'auto' in return type deduced as 'int' here but deduced as 'void' in earlier return statement}}
  };
}

auto test_static_assert_handler_result_mismatch(int value) {
  return match (value) {
    case 0 => static_assert(true);
    case _ => 1; // expected-error {{'auto' in return type deduced as 'int' here but deduced as 'void' in earlier return statement}}
  };
}

auto test_value_then_null_handler_result_mismatch(int value) {
  return match (value) {
    case 0 => 1;
    case _ => ; // expected-error {{'auto' in return type deduced as 'void' here but deduced as 'int' in earlier return statement}}
  };
}

void test_decomposition_pattern_arity() {
  struct S { int a; int b; };
  S s{1, 2};
  match (s) {
    case [1, 2, 3] => 0; // expected-error {{type 'S' binds to 2 elements, but 3 names were provided}}
    case _ => 0;
  }
}

namespace declaration_patterns {

struct Pair {
  int first;
  int second;
};

struct EmptyDecomposition {};

constexpr int empty_decomposition_pattern(EmptyDecomposition value) {
  return match (value) {
    case [] => 42;
  };
}

static_assert(empty_decomposition_pattern({}) == 42);
static_assert(match(EmptyDecomposition{}, case []));

int nonempty_decomposition_pattern(Pair value) {
  return match (value) {
    case [] => 0; // expected-error {{type 'Pair' binds to 2 elements, but no names were provided}}
    case _ => 1;
  };
}

struct Guarded {
  int value;
};

struct Movable {
  int value;
  Movable(const Movable &);
  Movable(Movable &&);
};

struct MovePair {
  Movable first;
  Movable second;
};

struct TriviallyMovable {
  int value;
};

struct Shape {
  virtual ~Shape();
};

struct Circle : Shape {
  int radius;
};

struct SpecialCircle : Circle {};

struct Square : Shape {
  int width;
};

bool sees_mutable(Guarded &);
bool sees_mutable(const Guarded &) = delete;

int basic(int value) {
  return match (value) {
    case int copy => copy;
  };
}

int reference(int &value) {
  return match (value) {
    case int &ref => ++ref;
  };
}

int condition(int value) {
  if (case int copy = value)
    return copy;
  return -1;
}

int case_condition(int value) {
  if (case int copy = value)
    return copy;
  return -1;
}

template <class T>
int dependent_case_condition(T value) {
  if (case int copy = value) // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'double'}}
    return copy;
  return -1;
}

int dependent_case_condition_ok = dependent_case_condition(1);
int dependent_case_condition_error =
    dependent_case_condition(1.0); // expected-note {{in instantiation of function template specialization 'declaration_patterns::dependent_case_condition<double>' requested here}}

int polymorphic_reference(Shape &shape) {
  return match (shape) {
    case Circle &circle => circle.radius;
    case Square &square => square.width;
    case _ => -1;
  };
}

int polymorphic_const_reference(const Shape &shape) {
  return match (shape) {
    case const Circle &circle => circle.radius;
    case const Square &square => square.width;
    case _ => -1;
  };
}

int polymorphic_rvalue_reference(Shape &&shape) {
  return match (static_cast<Shape &&>(shape)) {
    case Circle &&circle => circle.radius;
    case Square &&square => square.width;
    case _ => -1;
  };
}

int polymorphic_pointer_is_not_refined(Shape *shape) {
  return match (shape) {
    // expected-error@+1 {{declaration pattern of type 'Circle *' is not an exact match for subject of type 'Shape *'}}
    case Circle *circle => circle->radius;
    case _ => -1;
  };
}

int static_pointer_lvalue_reference(Circle *circle) {
  return match (circle) {
    case Circle *&ref => ref->radius;
  };
}

struct Erased {};

template<class T>
T* try_cast(Erased&);

int naked_declaration_does_not_use_adl_try_cast(Erased& erased) {
  return match (erased) {
    case int& value => value; // expected-error {{declaration pattern of type 'int &' is not an exact match for subject of type 'Erased'}}
    case _ => 0;
  };
}

int forwarding(int &&value) {
  return match (static_cast<int &&>(value)) {
    case auto &&ref => ref;
  };
}

int decomposition(Pair pair) {
  return match (pair) {
    case auto [first, second] => first + second;
  };
}

int pattern_binding_cannot_be_used_in_its_pattern(Pair pair) {
  return match (pair) {
    case [int first, first] => 1; // expected-error {{pattern binding 'first' cannot be used within the pattern that introduces it}}
    case _ => 0;
  };
}

struct NestedPair {
  int first;
  Pair second;
};

int nested_pattern_binding_cannot_be_used_in_its_pattern(NestedPair pair) {
  return match (pair) {
    case [int first, [first, _]] => 1; // expected-error {{pattern binding 'first' cannot be used within the pattern that introduces it}}
    case _ => 0;
  };
}

int attributed(int value) {
  return match (value) {
    case [[maybe_unused]] int copy => copy;
  };
}

int selection_value_patterns_are_constant(int subject, int value) { // expected-note {{declared here}}
  return match (subject) {
    case value => 1; // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
    case _ => 0;
  };
}

int side_effecting_selection_value_pattern(int subject, int value) { // expected-note {{declared here}}
  return match (subject) {
    case ++value => 1; // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
    case _ => 0;
  };
}

struct ConstantPatternPair {
  int first;
  int second;
};

int nested_selection_value_patterns_are_constant(ConstantPatternPair subject,
                                                  int value) { // expected-note {{declared here}}
  return match (subject) {
    case [value, _] => 1; // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
    case _ => 0;
  };
}

bool single_pattern_tests_require_constant_values(int subject,
                                                  int value) { // expected-note 2 {{declared here}}
  if (case value = subject) // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
    return true;
  return match(subject, case value); // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
}

template<class T>
int dependent_selection_value_pattern(int subject, T value) { // expected-note {{declared here}}
  return match (subject) {
    case value => 1; // expected-error {{pattern is not a constant expression}} expected-note {{function parameter 'value' with unknown value cannot be used in a constant expression}}
    case _ => 0;
  };
}

int instantiate_dependent_selection_value_pattern() {
  return dependent_selection_value_pattern(1, 1); // expected-note {{in instantiation of function template specialization}}
}

int attributed_decomposition(Pair pair) {
  return match (pair) {
    case [[maybe_unused]] auto [first, second] => first + second;
  };
}

int guard(int value) {
  return match (value) {
    case int copy if (copy > 0) => copy;
    case int copy => -copy;
  };
}

int mutable_guard(Guarded value) {
  return match (value) {
    case Guarded copy if (__is_same(decltype(copy), Guarded) &&
                     __is_same(decltype((copy)), Guarded &) &&
                     sees_mutable(copy)) => (copy.value = 1);
    case Guarded copy => copy.value;
  };
}

int guard_can_mutate_declaration(Guarded value) {
  return match (value) {
    case Guarded copy if ((copy.value = 1)) => copy.value;
    case _ => 0;
  };
}

int mutable_structured_binding_guard(Pair pair) {
  return match (pair) {
    case auto [first, second]
        if (__is_same(decltype(first), int) &&
            __is_same(decltype((first)), int &) && first > 0) =>
        (first = 0, second);
    case auto [first, second] => first + second;
  };
}

struct PointerMember {
  int *pointer;
};

int guard_can_mutate_pointee(PointerMember member) {
  return match (member) {
    case auto [pointer] if ((*pointer = 3, true)) => *pointer;
    case _ => 0;
  };
}

int guarded_move(Movable &&value) {
  return match (static_cast<Movable &&>(value)) {
    case Movable moved if (true) => moved.value; // expected-error {{guarded declaration pattern of type 'Movable' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_trivial_move(TriviallyMovable &&value) {
  return match (static_cast<TriviallyMovable &&>(value)) {
    case TriviallyMovable moved if (moved.value > 0) => moved.value;
    case _ => 0;
  };
}

int guarded_scalar_move(int &&value) {
  return match (static_cast<int &&>(value)) {
    case int moved if (moved < 0) => -moved;
    case int moved => moved;
  };
}

int guarded_structured_binding_move(MovePair &&value) {
  return match (static_cast<MovePair &&>(value)) {
    case auto [first, second] if (true) => first.value + second.value; // expected-error {{guarded declaration pattern of type 'MovePair' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_move(Movable (&&value)[1]) {
  return match (static_cast<Movable (&&)[1]>(value)) {
    case auto [element] if (true) => element.value; // expected-error {{guarded declaration pattern of type 'Movable[1]' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_nested_array_structured_binding_move(Movable (&&value)[1][1]) {
  return match (static_cast<Movable (&&)[1][1]>(value)) {
    case auto [row] if (true) => row[0].value; // expected-error {{guarded declaration pattern of type 'Movable[1][1]' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_copy(Movable (&value)[1]) {
  return match (value) {
    case auto [element] if (true) => element.value;
    case _ => 0;
  };
}

int unguarded_array_structured_binding_move(Movable (&&value)[1]) {
  return match (static_cast<Movable (&&)[1]>(value)) {
    case auto [element] => element.value;
  };
}

int unguarded_move(Movable &&value) {
  return match (static_cast<Movable &&>(value)) {
    case Movable moved => moved.value;
  };
}

int guarded_rvalue_reference(Movable &&value) {
  return match (static_cast<Movable &&>(value)) {
    case Movable &&ref if (ref.value > 0) => ref.value;
    case _ => 0;
  };
}

template<class T>
int forwarding_guard(T &&value) {
  return match (static_cast<T &&>(value)) {
    case T copy if (true) => 1; // expected-error {{guarded declaration pattern of type 'declaration_patterns::Movable' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int instantiate_forwarding_guard(Movable &lvalue, Movable &&rvalue) {
  return forwarding_guard(lvalue) +
         forwarding_guard(static_cast<Movable &&>(rvalue)); // expected-note {{in instantiation of function template specialization}}
}

template<class T>
T dependent(T value) {
  return match (value) {
    case T copy => copy;
  };
}

template<class T>
constexpr T dependent_guard(T value) {
  return match (value) {
    case T copy if (__is_same(decltype(copy), T) &&
               __is_same(decltype((copy)), T &) && copy > T{}) => copy;
    case T copy => copy;
  };
}

template<class T>
constexpr int dependent_decomposition_guard(T value) {
  return match (value) {
    case auto [first, second]
        if (__is_same(decltype((first)), int &) && first > 0) =>
        first + second;
    case _ => 0;
  };
}

static_assert(dependent_guard(3) == 3);
static_assert(dependent_decomposition_guard(Pair{2, 3}) == 5);
static_assert(dependent_decomposition_guard(0) == 0);

template<class T>
concept MatchesIntDeclaration = requires(T value) {
  match(value, case int copy);
};

template<class T>
concept MatchesPairDecomposition = requires(T value) {
  match(value, case auto &&[first, second]);
};

static_assert(MatchesIntDeclaration<int>);
static_assert(!MatchesIntDeclaration<double>);
static_assert(MatchesPairDecomposition<Pair>);
static_assert(!MatchesPairDecomposition<int>);

template<class T>
bool dependent_single_match(T value) {
  return match(value, case int copy); // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'double'}}
}

bool instantiate_invalid_single_match() {
  return dependent_single_match(0.0); // expected-note {{in instantiation of function template specialization 'declaration_patterns::dependent_single_match<double>' requested here}}
}

template<class T>
T dependent_auto(T value) {
  return match (value) {
    case auto &&ref => ref;
  };
}

static_assert(__is_same(decltype(dependent(1)), int));
static_assert(__is_same(decltype(dependent_auto(1)), int));

struct DispatchClass {};

template<class T>
constexpr int dependent_declaration_dispatch(T value) {
  return match (value) {
    case int i => i + 10;
    case char c => c == '1' ? 20 : 21;
    case DispatchClass object => static_cast<int>(sizeof(object));
  };
}

static_assert(dependent_declaration_dispatch(1) == 11);
static_assert(dependent_declaration_dispatch('1') == 20);
static_assert(dependent_declaration_dispatch(DispatchClass{}) == 1);

template<class U>
constexpr int dependent_pattern_type_dispatch(int value) {
  return match (value) {
    case U copy => 1;
    case _ => 0;
  };
}

static_assert(dependent_pattern_type_dispatch<int>(1) == 1);
static_assert(dependent_pattern_type_dispatch<double>(1) == 0);

struct DeletedCopy {
  DeletedCopy();
  DeletedCopy(const DeletedCopy&) = delete; // expected-note {{has been explicitly marked deleted here}}
};

template<class T>
int dependent_deleted_copy_does_not_fall_back(T& value) {
  return match (value) {
    case DeletedCopy copy => 1; // expected-error {{call to deleted constructor of 'DeletedCopy'}}
    case _ => 0;
  };
}

int instantiate_dependent_deleted_copy(DeletedCopy& value) {
  return dependent_deleted_copy_does_not_fall_back(value); // expected-note {{in instantiation of function template specialization 'declaration_patterns::dependent_deleted_copy_does_not_fall_back<declaration_patterns::DeletedCopy>' requested here}}
}

constexpr void selected_static_assert_handler(auto value) {
  match (value) {
    case int => ;
    case _ => static_assert(false, "selected static assertion handler"); // expected-error {{static assertion failed: selected static assertion handler}}
  }
}

void instantiate_selected_static_assert_handler() {
  selected_static_assert_handler(0.0); // expected-note {{in instantiation of function template specialization}}
}

struct DependentHandlerResult {
  constexpr unsigned long size() const { return 5; }
};

constexpr auto dependent_handler_result(auto value) {
  return match (value) {
    case int i => i;
    case DependentHandlerResult result => result.size();
    case _ => static_assert(false, "unsupported match subject");
  };
}

static_assert(dependent_handler_result(0) == 0);
static_assert(dependent_handler_result(DependentHandlerResult{}) == 5);
static_assert(__is_same(decltype(dependent_handler_result(0)), int));
static_assert(__is_same(
    decltype(dependent_handler_result(DependentHandlerResult{})),
    unsigned long));

constexpr auto dependent_runtime_result_mismatch(auto value) {
  return match (value) {
    case 0 => 1;
    case _ => 2.0; // expected-error {{'auto' in return type deduced as 'double' here but deduced as 'int' in earlier return statement}}
  };
}

constexpr auto instantiate_dependent_runtime_result_mismatch = dependent_runtime_result_mismatch(0); // expected-note {{in instantiation of function template specialization}}

struct BindingPackTriple {
  int first;
  int second;
  int third;
};

constexpr int binding_pack_sum(BindingPackTriple value) {
  return match (value) {
    case auto [...elements] => (... + elements);
  };
}

static_assert(binding_pack_sum({1, 2, 3}) == 6);

constexpr int binding_pack_case_condition(BindingPackTriple value) {
  if (case auto [...elements] = value)
    return (... + elements);
  return 0;
}

static_assert(binding_pack_case_condition({1, 2, 3}) == 6);

constexpr int unnamed_binding_pack(BindingPackTriple value) {
  return match (value) {
    case auto [first, ..., last] => first + last;
  };
}

static_assert(unnamed_binding_pack({1, 2, 3}) == 4);

constexpr int fully_unnamed_binding_pack(BindingPackTriple value) {
  return match (value) {
    case auto [...] => 1;
  };
}

static_assert(fully_unnamed_binding_pack({1, 2, 3}) == 1);

template<class T>
constexpr int dependent_unnamed_binding_pack(T value) {
  return match (value) -> int {
    case auto [first, ..., last] => first + last;
    case _ => -1;
  };
}

static_assert(dependent_unnamed_binding_pack(BindingPackTriple{1, 2, 3}) == 4);
static_assert(dependent_unnamed_binding_pack(0) == -1);

struct DeclarationPackFour {
  int first;
  int second;
  int third;
  int fourth;
};

constexpr int declaration_subpattern_pack(DeclarationPackFour value) {
  return match (value) {
    case [auto&& first, auto&& ...middle, auto&& last] =>
        int(sizeof...(middle)) + first + (... + middle) + last;
  };
}

static_assert(declaration_subpattern_pack({1, 2, 3, 4}) == 12);

constexpr int parenthesized_declaration_subpattern_pack(
    DeclarationPackFour value) {
  return match (value) {
    case [auto&& first, (auto&& ...middle), auto&& last] =>
        int(sizeof...(middle)) + first + (... + middle) + last;
  };
}

static_assert(parenthesized_declaration_subpattern_pack({1, 2, 3, 4}) == 12);

constexpr int typed_declaration_subpattern_pack(DeclarationPackFour value) {
  return match (value) {
    case [int first, int ...middle, int last] =>
        first + (... + middle) + last;
  };
}

static_assert(typed_declaration_subpattern_pack({1, 2, 3, 4}) == 10);

constexpr int wildcard_subpattern_pack(DeclarationPackFour value) {
  return match (value) {
    case [auto&& first, ..._, auto&& last] => first + last;
  };
}

static_assert(wildcard_subpattern_pack({1, 2, 3, 4}) == 5);

constexpr int parenthesized_wildcard_subpattern_pack(DeclarationPackFour value) {
  return match (value) {
    case [auto&& first, (..._), auto&& last] => first + last;
  };
}

static_assert(parenthesized_wildcard_subpattern_pack({1, 2, 3, 4}) == 5);

constexpr int empty_declaration_subpattern_pack(Pair value) {
  return match (value) {
    case [auto&& first, auto&& ...middle, auto&& last] =>
        int(sizeof...(middle)) + first + last;
  };
}

static_assert(empty_declaration_subpattern_pack({1, 2}) == 3);

constexpr int empty_wildcard_subpattern_pack(Pair value) {
  return match (value) {
    case [auto&& first, ..._, auto&& last] => first + last;
  };
}

static_assert(empty_wildcard_subpattern_pack({1, 2}) == 3);

struct NestedDeclarationPack {
  int first;
  DeclarationPackFour nested;
  int last;
};

constexpr int nested_declaration_subpattern_pack(NestedDeclarationPack value) {
  return match (value) {
    case [auto&& first,
          [auto&& nested_first, auto&& ...middle, auto&& nested_last],
          auto&& last] => first + nested_first + (... + middle) + nested_last +
                         last;
  };
}

static_assert(
    nested_declaration_subpattern_pack({1, {2, 3, 4, 5}, 6}) == 21);

constexpr int nested_wildcard_subpattern_pack(NestedDeclarationPack value) {
  return match (value) {
    case [auto&& first, [auto&& nested_first, ..._, auto&& nested_last],
          auto&& last] => first + nested_first + nested_last + last;
  };
}

static_assert(nested_wildcard_subpattern_pack({1, {2, 3, 4, 5}, 6}) == 14);

constexpr int declaration_subpattern_pack_guard(DeclarationPackFour value) {
  return match (value) {
    case [auto&& first, auto&& ...middle, auto&& last]
        if ((... + middle) < 0) => -1;
    case [auto&& first, auto&& ...middle, auto&& last] =>
        first + (... + middle) + last;
  };
}

static_assert(declaration_subpattern_pack_guard({1, 2, 3, 4}) == 10);

constexpr int declaration_subpattern_pack_condition(DeclarationPackFour value) {
  if (case [auto&& first, auto&& ...middle, auto&& last] = value)
    return first + (... + middle) + last;
  return 0;
}

static_assert(declaration_subpattern_pack_condition({1, 2, 3, 4}) == 10);

template<class T>
constexpr int dependent_declaration_subpattern_pack_size(T value) {
  return match (value) -> int {
    case [auto&& ...elements] => int(sizeof...(elements));
    case _ => -1;
  };
}

static_assert(
    dependent_declaration_subpattern_pack_size(DeclarationPackFour{}) == 4);
static_assert(dependent_declaration_subpattern_pack_size(1) == -1);

template<class T>
constexpr int dependent_wildcard_subpattern_pack(T value) {
  return match (value) -> int {
    case [..._] => 0;
    case _ => -1;
  };
}

static_assert(dependent_wildcard_subpattern_pack(DeclarationPackFour{}) == 0);
static_assert(dependent_wildcard_subpattern_pack(1) == -1);

int multiple_declaration_subpattern_packs(Pair value) {
  return match (value) {
    case [auto&& ...first, auto&& ...second] => 0; // expected-error {{multiple arity-inferred packs in decomposition pattern}} expected-note {{previous binding pack specified here}}
  };
}

int multiple_mixed_subpattern_packs(Pair value) {
  return match (value) {
    case [..._, auto&& ...middle] => 0; // expected-error {{multiple arity-inferred packs in decomposition pattern}} expected-note {{previous binding pack specified here}}
  };
}

int declaration_subpattern_pack_too_small(Pair value) {
  return match (value) {
    case [auto&& first, auto&& second, auto&& ...middle, auto&& last] => 0; // expected-error {{type 'Pair' decomposes into 2 elements, but decomposition pattern requires at least 3}}
  };
}

void binding_pack_loop_conditions(BindingPackTriple value) {
  while (case auto [...elements] = value) {
    (void)sizeof...(elements);
    break;
  }
  for (; case auto [...elements] = value;
       (void)sizeof...(elements)) {
    break;
  }
  while (match(value, case auto [...elements])) {
    break;
  }
  for (; match(value, case auto [...elements]);
       (void)0) {
    break;
  }
}

template<class T>
constexpr int dependent_binding_pack_size(T value) {
  return match (value) -> int {
    case auto [...elements] => int(sizeof...(elements));
    case _ => -1;
  };
}

struct EmptyBindingPack {};

static_assert(dependent_binding_pack_size(EmptyBindingPack{}) == 0);
static_assert(dependent_binding_pack_size(Pair{1, 2}) == 2);
static_assert(dependent_binding_pack_size(1) == -1);

int binding_pack_does_not_alias_a_fixed_arity_decomposition(Pair &value) {
  return match (value) {
    case auto &&[...elements] if (false) => 0;
    case auto &&[element] => element; // expected-error {{type 'Pair' binds to 2 elements, but only 1 name was provided}}
    case _ => -1;
  };
}

int bad_conversion(int value) {
  return match (value) {
    case char converted => converted; // expected-error {{declaration pattern of type 'char' is not an exact match for subject of type 'int'}}
    case _ => 0;
  };
}

int bad_promotion(char value) {
  return match (value) {
    case int promoted => promoted; // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'char'}}
    case _ => 0;
  };
}

int bad_storage(int value) {
  return match (value) {
    case static int copy => copy; // expected-error {{loop variable 'copy' may not be declared 'static'}}
    case _ => 0;
  };
}

} // namespace declaration_patterns
