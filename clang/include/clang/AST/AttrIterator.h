//===- AttrIterator.h - Classes for attribute iteration ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Attr vector and specific_attr_iterator interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_ATTRITERATOR_H
#define LLVM_CLANG_AST_ATTRITERATOR_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/ADL.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace clang {

class Attr;

/// AttrVec - A vector of Attr, which is how they are stored on the AST.
using AttrVec = SmallVector<Attr *, 4>;

/// specific_attr_iterator - Iterates over a subrange of an AttrVec, only
/// providing attributes that are of a specific type.
template <typename SpecificAttr, typename Container = AttrVec>
class specific_attr_iterator {
  using Iterator = typename Container::const_iterator;

  /// Current - The current, underlying iterator.
  /// In order to ensure we don't dereference an invalid iterator unless
  /// specifically requested, we don't necessarily advance this all the
  /// way. Instead, we advance it when an operation is requested; if the
  /// operation is acting on what should be a past-the-end iterator,
  /// then we offer no guarantees, but this way we do not dereference a
  /// past-the-end iterator when we move to a past-the-end position.
  mutable Iterator Current;

  void AdvanceToNext() const {
    while (!isa<SpecificAttr>(*Current))
      ++Current;
  }

  void AdvanceToNext(Iterator I) const {
    while (Current != I && !isa<SpecificAttr>(*Current))
      ++Current;
  }

public:
  using value_type = SpecificAttr *;
  using reference = SpecificAttr *;
  using pointer = SpecificAttr *;
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;

  specific_attr_iterator() = default;
  explicit specific_attr_iterator(Iterator i) : Current(i) {}

  reference operator*() const {
    AdvanceToNext();
    return cast<SpecificAttr>(*Current);
  }
  pointer operator->() const {
    AdvanceToNext();
    return cast<SpecificAttr>(*Current);
  }

  specific_attr_iterator& operator++() {
    ++Current;
    return *this;
  }
  specific_attr_iterator operator++(int) {
    specific_attr_iterator Tmp(*this);
    ++(*this);
    return Tmp;
  }

  friend bool operator==(specific_attr_iterator Left,
                         specific_attr_iterator Right) {
    assert((Left.Current == nullptr) == (Right.Current == nullptr));
    if (Left.Current < Right.Current)
      Left.AdvanceToNext(Right.Current);
    else
      Right.AdvanceToNext(Left.Current);
    return Left.Current == Right.Current;
  }
  friend bool operator!=(specific_attr_iterator Left,
                         specific_attr_iterator Right) {
    return !(Left == Right);
  }
};

template <typename SpecificAttr, typename Container>
inline specific_attr_iterator<SpecificAttr, Container>
          specific_attr_begin(const Container& container) {
  return specific_attr_iterator<SpecificAttr, Container>(container.begin());
}
template <typename SpecificAttr, typename Container>
inline specific_attr_iterator<SpecificAttr, Container>
          specific_attr_end(const Container& container) {
  return specific_attr_iterator<SpecificAttr, Container>(container.end());
}

template <typename SpecificAttr, typename Container>
inline bool hasSpecificAttr(const Container& container) {
  return specific_attr_begin<SpecificAttr>(container) !=
          specific_attr_end<SpecificAttr>(container);
}
template <typename SpecificAttr, typename Container>
inline auto *getSpecificAttr(const Container &container) {
  using ValueTy = llvm::detail::ValueOfRange<Container>;
  using ValuePointeeTy = std::remove_pointer_t<ValueTy>;
  using IterTy = std::conditional_t<std::is_const_v<ValuePointeeTy>,
                                    const SpecificAttr, SpecificAttr>;
  auto It = specific_attr_begin<IterTy>(container);
  return It != specific_attr_end<IterTy>(container) ? *It : nullptr;
}

template <typename SpecificAttr, typename Container>
inline auto getSpecificAttrs(const Container &container) {
  using ValueTy = llvm::detail::ValueOfRange<Container>;
  using ValuePointeeTy = std::remove_pointer_t<ValueTy>;
  using IterTy = std::conditional_t<std::is_const_v<ValuePointeeTy>,
                                    const SpecificAttr, SpecificAttr>;
  auto Begin = specific_attr_begin<IterTy>(container);
  auto End = specific_attr_end<IterTy>(container);
  return llvm::make_range(Begin, End);
}

} // namespace clang

#endif // LLVM_CLANG_AST_ATTRITERATOR_H
