// RUN: %clang_cc1 -std=c++2d -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

struct Pair {
  int first;
  int second;
};

struct Outer {
  Pair pair;
  int last;
};

auto [[global_x, global_y], global_z] = Outer{{1, 2}, 3};
auto [[pack_first, ..., pack_last], pack_tail] = Outer{{4, 5}, 6};

// CHECK: @_ZDC8global_x8global_y8global_zE = global %struct.Outer
// CHECK: @_ZDC10pack_first9pack_last9pack_tailE = global %struct.Outer
// CHECK: @_ZDC7tuple_x7tuple_y7tuple_zE = global %struct.TupleOuter
// CHECK: @[[HOLDER1:_ZL[0-9]+__nested_structured_binding_[0-9]+]] = internal global %struct.TupleInner zeroinitializer
// CHECK: @_ZDC7other_x7other_y7other_zE = global %struct.TupleOuter
// CHECK: @[[HOLDER2:_ZL[0-9]+__nested_structured_binding_[0-9]+]] = internal global %struct.TupleInner zeroinitializer
// CHECK: @{{.*}}inline_local_address{{.*}}{{[0-9]+}}__nested_structured_binding_{{[0-9]+}} = linkonce_odr

int sum() {
  return global_x + global_y + global_z;
}

int pack_sum() {
  return pack_first + pack_last + pack_tail;
}

// CHECK-LABEL: define {{.*}} @_Z3sumv()
// CHECK: load i32, ptr @_ZDC8global_x8global_y8global_zE
// CHECK: load i32, ptr getelementptr inbounds nuw (i8, ptr @_ZDC8global_x8global_y8global_zE, i64 4)
// CHECK: load i32, ptr getelementptr inbounds nuw (i8, ptr @_ZDC8global_x8global_y8global_zE, i64 8)

namespace std {
using size_t = decltype(sizeof(0));
template<class> struct tuple_size;
template<size_t, class> struct tuple_element;
}

struct TupleInner {
  int first;
  int second;
};

struct TupleOuter {
  TupleInner inner;
  int last;
};

template<std::size_t I>
decltype(auto) get(TupleInner&& value) {
  if constexpr (I == 0)
    return static_cast<int&&>(value.first);
  else
    return static_cast<int&&>(value.second);
}

template<std::size_t I>
decltype(auto) get(TupleOuter&& value) {
  if constexpr (I == 0)
    return TupleInner{value.inner.first, value.inner.second};
  else
    return static_cast<int&&>(value.last);
}

template<> struct std::tuple_size<TupleInner> {
  static constexpr size_t value = 2;
};
template<std::size_t I> struct std::tuple_element<I, TupleInner> {
  using type = int;
};
template<> struct std::tuple_size<TupleOuter> {
  static constexpr size_t value = 2;
};
template<> struct std::tuple_element<0, TupleOuter> {
  using type = TupleInner;
};
template<> struct std::tuple_element<1, TupleOuter> {
  using type = int;
};

auto [[tuple_x, tuple_y], tuple_z] = TupleOuter{{4, 5}, 6};
auto [[other_x, other_y], other_z] = TupleOuter{{7, 8}, 9};

inline int *inline_local_address() {
  static auto [[local_x, local_y], local_z] = TupleOuter{{1, 2}, 3};
  return &local_x;
}

int *use_inline_local_address() {
  return inline_local_address();
}

int tuple_sum() {
  return tuple_x + tuple_y + tuple_z;
}
