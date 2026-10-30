//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, no-rtti

#include "../src/private_typeinfo.h"

#include <cassert>
#include <cstddef>
#include <typeinfo>

using namespace __cxxabiv1;

template <class T>
const __class_type_info* class_type() {
  return static_cast<const __class_type_info*>(&typeid(T));
}

template <class... Targets, class Source>
void check_casts(Source* source, const std::ptrdiff_t* hints = nullptr) {
  __dynamic_cast_target targets[] = {{class_type<Targets>(), -1}...};
  const void* results[]           = {dynamic_cast<const Targets*>(source)...};
  const size_t count              = sizeof...(Targets);
  size_t expected                 = count;
  for (size_t i = 0; i < count; ++i) {
    if (hints != nullptr)
      targets[i].src2dst_offset = hints[i];
    if (expected == count && results[i] != nullptr)
      expected = i;
  }

  size_t selected    = static_cast<size_t>(-1);
  const void* result = __dynamic_cast_first(source, class_type<Source>(), targets, count, &selected);
  assert(selected == expected);
  assert(result == (expected == count ? nullptr : results[expected]));
}

namespace single_inheritance {
struct Shape {
  virtual ~Shape() {}
};
struct Circle : Shape {};
struct Triangle : Shape {};
struct Rectangle : Shape {};
struct Square : Rectangle {};
struct Disk : Circle {};
struct Other : Shape {};

void test() {
  Shape shape;
  Circle circle;
  Triangle triangle;
  Rectangle rectangle;
  Square square;
  Disk disk;
  Other other;
  const std::ptrdiff_t hints[] = {0, 0, 0};
  Shape* sources[]             = {&shape, &circle, &triangle, &rectangle, &square, &disk, &other};
  for (Shape* source : sources) {
    check_casts<Circle, Triangle, Rectangle>(source);
    check_casts<Circle, Triangle, Rectangle>(source, hints);
  }

  // Square is not a target, but its Rectangle base matches the last target.
  Shape* square_shape = &square;
  assert(dynamic_cast<Rectangle*>(square_shape) == static_cast<Rectangle*>(&square));
  check_casts<Circle, Triangle, Rectangle>(static_cast<const Shape*>(&square));
  check_casts<Rectangle, Square, Circle>(square_shape);
  check_casts<Square, Rectangle, Circle>(square_shape);
  check_casts<Rectangle, Square, Circle>(square_shape, hints);
  check_casts<Square, Rectangle, Circle>(square_shape, hints);

  // Traversal order is most-derived first, but selection follows target order.
  check_casts<Circle, Disk, Rectangle>(static_cast<Shape*>(&disk));
  check_casts<Disk, Circle, Rectangle>(static_cast<Shape*>(&disk));
  check_casts<Circle, Disk, Rectangle>(static_cast<Shape*>(&disk), hints);
  check_casts<Disk, Circle, Rectangle>(static_cast<Shape*>(&disk), hints);
  const std::ptrdiff_t mixed_hints[] = {-1, 0, 0};
  check_casts<Circle, Disk, Rectangle>(static_cast<Shape*>(&disk), mixed_hints);
  check_casts<Triangle, Circle, Circle>(static_cast<const Shape*>(&disk));
  check_casts<Circle, Triangle, Rectangle>(static_cast<Shape*>(nullptr));
  check_casts<Circle>(static_cast<Shape*>(&circle), hints);
  check_casts<Circle>(static_cast<Shape*>(&disk), hints);
  check_casts<Circle>(static_cast<Shape*>(nullptr), hints);

  size_t selected = static_cast<size_t>(-1);
  assert(__dynamic_cast_first(&circle, class_type<Shape>(), nullptr, 0, &selected) == nullptr);
  assert(selected == 0);
  selected = static_cast<size_t>(-1);
  assert(__dynamic_cast_first(nullptr, nullptr, nullptr, 0, &selected) == nullptr);
  assert(selected == 0);
}
} // namespace single_inheritance

namespace multiple_inheritance {
using namespace single_inheritance;
struct Both : Circle, Triangle {};
struct More : Both {};

void test() {
  Both both;
  Shape* circle   = static_cast<Circle*>(&both);
  Shape* triangle = static_cast<Triangle*>(&both);
  assert(static_cast<const void*>(circle) != static_cast<const void*>(triangle));
  check_casts<Circle, Triangle, Rectangle>(circle);
  check_casts<Triangle, Circle, Rectangle>(circle);
  check_casts<Circle, Triangle, Rectangle>(triangle);
  check_casts<Triangle, Circle, Rectangle>(triangle);
  check_casts<Rectangle, Both>(triangle);

  // A hint-proven later target cannot bypass an earlier successful crosscast.
  const std::ptrdiff_t hints[] = {-2, 0};
  check_casts<Triangle, Both>(static_cast<Circle*>(&both), hints);
  check_casts<Rectangle, Both>(static_cast<Circle*>(&both), hints);

  // Exact dynamic-type matches can also use a nonzero source-to-target offset.
  const std::ptrdiff_t offset =
      reinterpret_cast<const char*>(static_cast<Triangle*>(&both)) - reinterpret_cast<const char*>(&both);
  assert(offset != 0);
  const std::ptrdiff_t nonzero_hint[] = {offset};
  check_casts<Both>(static_cast<Triangle*>(&both), nonzero_hint);
  const std::ptrdiff_t later_nonzero_hint[] = {-2, offset};
  check_casts<Circle, Both>(static_cast<Triangle*>(&both), later_nonzero_hint);
  check_casts<Rectangle, Both>(static_cast<Triangle*>(&both), later_nonzero_hint);

  // The same hint is useful for a target reached through a shared chain, not
  // just for the exact dynamic type.
  More more;
  const std::ptrdiff_t zero_hint[] = {0};
  check_casts<Both>(static_cast<Circle*>(&more), zero_hint);
  check_casts<Triangle, Both>(static_cast<Circle*>(&more), hints);
  check_casts<Rectangle, Both>(static_cast<Circle*>(&more), hints);
}
} // namespace multiple_inheritance

namespace virtual_inheritance {
struct Root {
  virtual ~Root() {}
};
struct A : virtual Root {};
struct Left : A {};
struct Right : A {};
struct Object : Left, Right {};

void test() {
  Object object;
  Root* source = &object;
  assert(dynamic_cast<A*>(source) == nullptr);
  check_casts<A>(source);
  check_casts<A, Left, Right>(source);
  check_casts<A, Right, Left>(source);
  check_casts<Object, A>(source);
}
} // namespace virtual_inheritance

namespace repeated_nonvirtual_base {
struct Root {
  virtual ~Root() {}
};
struct A : Root {};
struct Left : A {};
struct Right : A {};
struct Object : Left, Right {};

void test() {
  Object object;
  Root* left  = static_cast<Left*>(&object);
  Root* right = static_cast<Right*>(&object);
  assert(dynamic_cast<A*>(left) == static_cast<A*>(static_cast<Left*>(&object)));
  assert(dynamic_cast<A*>(right) == static_cast<A*>(static_cast<Right*>(&object)));
  check_casts<A, Left, Right>(left);
  check_casts<A, Left, Right>(right);
  check_casts<Right, Left>(left);
  check_casts<Left, Right>(right);
  const std::ptrdiff_t hint[] = {-3};
  check_casts<Object>(left, hint);
  check_casts<Object>(right, hint);
}
} // namespace repeated_nonvirtual_base

namespace private_nonvirtual_base {
struct Root {
  virtual ~Root() {}
};
struct Hidden : private Root {
  Root* root() { return this; }
};
struct Public : Root {};
struct Object : Hidden, Public {};

void test() {
  Object object;
  Root* public_base = static_cast<Public*>(&object);
  Root* hidden_base = object.root();
  assert(dynamic_cast<Object*>(hidden_base) == nullptr);

  // The hint identifies the unique public base, not the private subobject of
  // the same type. An exact dynamic-type match must still check this offset.
  const std::ptrdiff_t hint[] = {reinterpret_cast<const char*>(public_base) - reinterpret_cast<const char*>(&object)};
  assert(hint[0] != 0);
  check_casts<Object>(public_base, hint);
  check_casts<Object>(hidden_base, hint);
}
} // namespace private_nonvirtual_base

namespace mixed_access {
struct Root {
  virtual ~Root() {}
};
struct Hidden : protected virtual Root {};
struct Object : Hidden, virtual Root {};
struct Other {
  virtual ~Other() {}
};

void test() {
  Object object;
  Root* source = &object;
  check_casts<Other, Hidden, Object>(source);
  check_casts<Other, Object, Hidden>(source);
  const std::ptrdiff_t hints[] = {-2, -1};
  check_casts<Other, Hidden>(source, hints);
}
} // namespace mixed_access

int main(int, char**) {
  single_inheritance::test();
  multiple_inheritance::test();
  virtual_inheritance::test();
  repeated_nonvirtual_base::test();
  private_nonvirtual_base::test();
  mixed_access::test();
  return 0;
}
