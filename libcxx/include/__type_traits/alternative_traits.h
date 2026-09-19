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
    if (info != meta::info{}) {
      if (meta::is_type(info)) {
        if (empty)
          throw "a typed alternative cannot be empty";
      } else {
        info = meta::constant_of(info);
      }
    }
  }
};

template <class _Tp>
struct alternative_traits;

template <class _Tp>
struct alternative_traits<_Tp*> {
  static constexpr alternative_info alternatives[] = {
      {meta::reflect_constant(nullptr), /*empty=*/true},
      ^^_Tp,
  };
  static constexpr bool has_residual_states = false;

  enum class state : bool { empty = false, value = true };

  // The parameter is templated so nullable library types can reuse this
  // implementation while operating on the actual matching subject.
  template <class _Self>
  _LIBCPP_HIDE_FROM_ABI static constexpr state index(const _Self& __self) noexcept {
    return __self ? state::value : state::empty;
  }

  template <state _State, class _Self>
    requires(_State == state::value)
  _LIBCPP_HIDE_FROM_ABI static constexpr decltype(auto) get(_Self&& __self) noexcept {
    return match constexpr (is_void_v<_Tp>) -> decltype(auto) {
      case true => ;
      case false => *std::forward<_Self>(__self);
    };
  }
};

#endif // _LIBCPP_STD_VER >= 29 && __has_feature(pattern_matching)

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_ALTERNATIVE_TRAITS_H
