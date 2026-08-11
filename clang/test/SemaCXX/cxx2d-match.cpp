// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching -fcxx-exceptions -Wno-unused-variable -Wno-unused-value %s -verify

void test_throw_does_not_contribute_to_type_deduction() {
  static_assert(__is_same(decltype(0 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  }), int));
}

void test_throw_action() {
  static_assert(0 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 0);
  static_assert(1 match {
    case 0 => 0;
    case 1 => 1;
    case _ => throw;
  } == 1);
}

constexpr void test_null_and_static_assert_handlers(bool value) {
  value match {
    case true => ;
    case false => static_assert(sizeof(int) >= 2);
  };
}

static_assert((test_null_and_static_assert_handlers(true), true));
static_assert((test_null_and_static_assert_handlers(false), true));

auto test_null_handler_result_mismatch(int value) {
  return value match {
    case 0 => ;
    case _ => 1; // expected-error {{'auto' in return type deduced as 'int' here but deduced as 'void' in earlier return statement}}
  };
}

auto test_static_assert_handler_result_mismatch(int value) {
  return value match {
    case 0 => static_assert(true);
    case _ => 1; // expected-error {{'auto' in return type deduced as 'int' here but deduced as 'void' in earlier return statement}}
  };
}

auto test_value_then_null_handler_result_mismatch(int value) {
  return value match {
    case 0 => 1;
    case _ => ; // expected-error {{'auto' in return type deduced as 'void' here but deduced as 'int' in earlier return statement}}
  };
}

void test_decomposition_pattern_arity() {
  struct S { int a; int b; };
  S s{1, 2};
  s match {
    case [1, 2, 3] => 0; // expected-error {{type 'S' binds to 2 elements, but 3 names were provided}}
    case _ => 0;
  };
}

namespace declaration_patterns {

struct Pair {
  int first;
  int second;
};

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
  return value match {
    case int copy => copy;
  };
}

int reference(int &value) {
  return value match {
    case int &ref => ++ref;
  };
}

int condition(int value) {
  if (value match case int copy)
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
  return shape match {
    case Circle &circle => circle.radius;
    case Square &square => square.width;
    case _ => -1;
  };
}

int polymorphic_const_reference(const Shape &shape) {
  return shape match {
    case const Circle &circle => circle.radius;
    case const Square &square => square.width;
    case _ => -1;
  };
}

int polymorphic_rvalue_reference(Shape &&shape) {
  return static_cast<Shape &&>(shape) match {
    case Circle &&circle => circle.radius;
    case Square &&square => square.width;
    case _ => -1;
  };
}

int polymorphic_pointer(Shape *shape) {
  return shape match {
    case Circle *circle => circle->radius;
    case Square *square => square->width;
    case _ => -1;
  };
}

int polymorphic_pointer_const_reference(Shape *shape) {
  return shape match {
    case Circle *const &circle => circle->radius;
    case _ => -1;
  };
}

int polymorphic_pointer_rvalue_reference(Shape *shape) {
  return shape match {
    case Circle *&&circle => circle->radius;
    case _ => -1;
  };
}

int polymorphic_pointer_lvalue_reference(Shape *shape) {
  return shape match {
    // expected-error@+1 {{non-const lvalue reference to type 'Circle *' cannot bind to a temporary of type 'Circle *'}}
    case Circle *&circle => circle->radius;
    case _ => -1;
  };
}

int static_pointer_lvalue_reference(Circle *circle) {
  return circle match {
    case Circle *&ref => ref->radius;
  };
}

struct Erased {};

template<class T>
T* try_cast(Erased&);

int naked_declaration_does_not_use_adl_try_cast(Erased& erased) {
  return erased match {
    case int& value => value; // expected-error {{declaration pattern of type 'int &' is not an exact match for subject of type 'Erased'}}
    case _ => 0;
  };
}

int forwarding(int &&value) {
  return static_cast<int &&>(value) match {
    case auto &&ref => ref;
  };
}

int decomposition(Pair pair) {
  return pair match {
    case auto [first, second] => first + second;
  };
}

int pattern_binding_cannot_be_used_in_its_pattern(Pair pair) {
  return pair match {
    case [int first, first] => 1; // expected-error {{pattern binding 'first' cannot be used within the pattern that introduces it}}
    case _ => 0;
  };
}

struct NestedPair {
  int first;
  Pair second;
};

int nested_pattern_binding_cannot_be_used_in_its_pattern(NestedPair pair) {
  return pair match {
    case [int first, [first, _]] => 1; // expected-error {{pattern binding 'first' cannot be used within the pattern that introduces it}}
    case _ => 0;
  };
}

int attributed(int value) {
  return value match {
    case [[maybe_unused]] int copy => copy;
  };
}

int attributed_decomposition(Pair pair) {
  return pair match {
    case [[maybe_unused]] auto [first, second] => first + second;
  };
}

int guard(int value) {
  return value match {
    case int copy if (copy > 0) => copy;
    case int copy => -copy;
  };
}

int mutable_guard(Guarded value) {
  return value match {
    case Guarded copy if (__is_same(decltype(copy), Guarded) &&
                     __is_same(decltype((copy)), Guarded &) &&
                     sees_mutable(copy)) => (copy.value = 1);
    case Guarded copy => copy.value;
  };
}

int guard_can_mutate_declaration(Guarded value) {
  return value match {
    case Guarded copy if ((copy.value = 1)) => copy.value;
    case _ => 0;
  };
}

int mutable_structured_binding_guard(Pair pair) {
  return pair match {
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
  return member match {
    case auto [pointer] if ((*pointer = 3, true)) => *pointer;
    case _ => 0;
  };
}

int guarded_move(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable moved if (true) => moved.value; // expected-error {{guarded declaration pattern of type 'Movable' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_trivial_move(TriviallyMovable &&value) {
  return static_cast<TriviallyMovable &&>(value) match {
    case TriviallyMovable moved if (moved.value > 0) => moved.value;
    case _ => 0;
  };
}

int guarded_scalar_move(int &&value) {
  return static_cast<int &&>(value) match {
    case int moved if (moved < 0) => -moved;
    case int moved => moved;
  };
}

int guarded_structured_binding_move(MovePair &&value) {
  return static_cast<MovePair &&>(value) match {
    case auto [first, second] if (true) => first.value + second.value; // expected-error {{guarded declaration pattern of type 'MovePair' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_move(Movable (&&value)[1]) {
  return static_cast<Movable (&&)[1]>(value) match {
    case auto [element] if (true) => element.value; // expected-error {{guarded declaration pattern of type 'Movable[1]' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_nested_array_structured_binding_move(Movable (&&value)[1][1]) {
  return static_cast<Movable (&&)[1][1]>(value) match {
    case auto [row] if (true) => row[0].value; // expected-error {{guarded declaration pattern of type 'Movable[1][1]' invokes a non-trivial move constructor before its guard; bind a reference and move in the handler instead}}
    case _ => 0;
  };
}

int guarded_array_structured_binding_copy(Movable (&value)[1]) {
  return value match {
    case auto [element] if (true) => element.value;
    case _ => 0;
  };
}

int unguarded_array_structured_binding_move(Movable (&&value)[1]) {
  return static_cast<Movable (&&)[1]>(value) match {
    case auto [element] => element.value;
  };
}

int unguarded_move(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable moved => moved.value;
  };
}

int guarded_rvalue_reference(Movable &&value) {
  return static_cast<Movable &&>(value) match {
    case Movable &&ref if (ref.value > 0) => ref.value;
    case _ => 0;
  };
}

template<class T>
int forwarding_guard(T &&value) {
  return static_cast<T &&>(value) match {
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
  return value match {
    case T copy => copy;
  };
}

template<class T>
constexpr T dependent_guard(T value) {
  return value match {
    case T copy if (__is_same(decltype(copy), T) &&
               __is_same(decltype((copy)), T &) && copy > T{}) => copy;
    case T copy => copy;
  };
}

template<class T>
constexpr int dependent_decomposition_guard(T value) {
  return value match {
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
  value match case int copy;
};

template<class T>
concept MatchesPairDecomposition = requires(T value) {
  value match case auto &&[first, second];
};

static_assert(MatchesIntDeclaration<int>);
static_assert(!MatchesIntDeclaration<double>);
static_assert(MatchesPairDecomposition<Pair>);
static_assert(!MatchesPairDecomposition<int>);

template<class T>
bool dependent_single_match(T value) {
  return value match case int copy; // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'double'}}
}

bool instantiate_invalid_single_match() {
  return dependent_single_match(0.0); // expected-note {{in instantiation of function template specialization 'declaration_patterns::dependent_single_match<double>' requested here}}
}

template<class T>
T dependent_auto(T value) {
  return value match {
    case auto &&ref => ref;
  };
}

static_assert(__is_same(decltype(dependent(1)), int));
static_assert(__is_same(decltype(dependent_auto(1)), int));

struct DispatchClass {};

template<class T>
constexpr int dependent_declaration_dispatch(T value) {
  return value match {
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
  return value match {
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
  return value match {
    case DeletedCopy copy => 1; // expected-error {{call to deleted constructor of 'DeletedCopy'}}
    case _ => 0;
  };
}

int instantiate_dependent_deleted_copy(DeletedCopy& value) {
  return dependent_deleted_copy_does_not_fall_back(value); // expected-note {{in instantiation of function template specialization 'declaration_patterns::dependent_deleted_copy_does_not_fall_back<declaration_patterns::DeletedCopy>' requested here}}
}

constexpr void selected_static_assert_handler(auto value) {
  value match {
    case int => ;
    case _ => static_assert(false, "selected static assertion handler"); // expected-error {{static assertion failed: selected static assertion handler}}
  };
}

void instantiate_selected_static_assert_handler() {
  selected_static_assert_handler(0.0); // expected-note {{in instantiation of function template specialization}}
}

struct BindingPackTriple {
  int first;
  int second;
  int third;
};

constexpr int binding_pack_sum(BindingPackTriple value) {
  return value match {
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

template<class T>
constexpr int dependent_binding_pack_size(T value) {
  return value match -> int {
    case auto [...elements] => int(sizeof...(elements));
    case _ => -1;
  };
}

struct EmptyBindingPack {};

static_assert(dependent_binding_pack_size(EmptyBindingPack{}) == 0);
static_assert(dependent_binding_pack_size(Pair{1, 2}) == 2);
static_assert(dependent_binding_pack_size(1) == -1);

int binding_pack_does_not_alias_a_fixed_arity_decomposition(Pair &value) {
  return value match {
    case auto &&[...elements] if (false) => 0;
    case auto &&[element] => element; // expected-error {{type 'Pair' binds to 2 elements, but only 1 name was provided}}
    case _ => -1;
  };
}

int bad_conversion(int value) {
  return value match {
    case char converted => converted; // expected-error {{declaration pattern of type 'char' is not an exact match for subject of type 'int'}}
    case _ => 0;
  };
}

int bad_promotion(char value) {
  return value match {
    case int promoted => promoted; // expected-error {{declaration pattern of type 'int' is not an exact match for subject of type 'char'}}
    case _ => 0;
  };
}

int bad_storage(int value) {
  return value match {
    case static int copy => copy; // expected-error {{loop variable 'copy' may not be declared 'static'}}
    case _ => 0;
  };
}

} // namespace declaration_patterns
