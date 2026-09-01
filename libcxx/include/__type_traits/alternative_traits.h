//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___TYPE_TRAITS_ALTERNATIVE_TRAITS_H
#define _LIBCPP___TYPE_TRAITS_ALTERNATIVE_TRAITS_H

#include <__config>
#include <__cstddef/size_t.h>
#include <__meta/core.h>
#include <__type_traits/is_void.h>
#include <__utility/forward.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER >= 29 && __has_feature(pattern_matching)

static_assert(__has_feature(reflection), "pattern matching library support requires reflection");

struct alternative_info {
  meta::info info = {};
  bool empty      = false;

  _LIBCPP_HIDE_FROM_ABI consteval alternative_info() noexcept = default;

  _LIBCPP_HIDE_FROM_ABI consteval alternative_info(meta::info __info, bool __empty = false)
      : info(__info), empty(__empty) {
    if (meta::is_type(info)) {
      if (empty)
        throw "a typed alternative cannot be empty";
    } else {
      info = meta::constant_of(info);
    }
  }
};

template <class _Tp>
struct alternative_traits;

template <class _Provider>
struct alternative_name {
  using provider = _Provider;

  size_t index;

  consteval alternative_name(size_t __index) : index(__index) {}
};

template <class _Tp>
struct alternative_traits<_Tp*> {
  using _AT = alternative_traits;

  static constexpr alternative_info alternatives[] = {
      {meta::reflect_constant(nullptr), /*empty=*/true},
      ^^_Tp,
  };
  static constexpr bool has_residual_states = false;

  // This provider is inherited by nullable types and also names a nullable
  // view of expected, so its operations act on the actual matching subject.
  template <class _Self>
  _LIBCPP_HIDE_FROM_ABI static constexpr bool index(const _Self& __self) noexcept {
    return __self ? true : false;
  }

  template <bool _HasValue, class _Self>
    requires(_HasValue)
  _LIBCPP_HIDE_FROM_ABI static constexpr decltype(auto) get(_Self&& __self) noexcept {
    if constexpr (is_void_v<_Tp>)
      return;
    else
      return *std::forward<_Self>(__self);
  }

  struct names {
    static constexpr alternative_name<_AT> none = 0, some = 1;
  };
};

#endif // _LIBCPP_STD_VER >= 29 && __has_feature(pattern_matching)

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_ALTERNATIVE_TRAITS_H
