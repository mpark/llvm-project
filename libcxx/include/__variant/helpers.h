//===---------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef _LIBCPP___VARIANT_HELPERS_H
#define _LIBCPP___VARIANT_HELPERS_H

#include <__cstddef/size_t.h>
#include <__fwd/variant.h>
#include <__type_traits/add_cv_quals.h>
#include <exception>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

namespace std { // explicitly not using versioning namespace

class _LIBCPP_EXPORTED_FROM_ABI _LIBCPP_AVAILABILITY_BAD_VARIANT_ACCESS bad_variant_access : public exception {
public:
  const char* what() const _NOEXCEPT override;
};

} // namespace std

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER >= 17

template <class _Tp>
#if _LIBCPP_STD_VER >= 26
requires requires { variant_size<_Tp>::value; }
#endif
struct _LIBCPP_TEMPLATE_VIS variant_size<const _Tp> : variant_size<_Tp> {};

template <class _Tp>
#if _LIBCPP_STD_VER >= 26
requires requires { variant_size<_Tp>::value; }
#endif
struct _LIBCPP_TEMPLATE_VIS variant_size<volatile _Tp> : variant_size<_Tp> {};

template <class _Tp>
#if _LIBCPP_STD_VER >= 26
requires requires { variant_size<_Tp>::value; }
#endif
struct _LIBCPP_TEMPLATE_VIS variant_size<const volatile _Tp> : variant_size<_Tp> {};

template <size_t _Ip, class _Tp>
struct _LIBCPP_TEMPLATE_VIS variant_alternative<_Ip, const _Tp> : add_const<variant_alternative_t<_Ip, _Tp>> {};

template <size_t _Ip, class _Tp>
struct _LIBCPP_TEMPLATE_VIS variant_alternative<_Ip, volatile _Tp> : add_volatile<variant_alternative_t<_Ip, _Tp>> {};

template <size_t _Ip, class _Tp>
struct _LIBCPP_TEMPLATE_VIS variant_alternative<_Ip, const volatile _Tp> : add_cv<variant_alternative_t<_Ip, _Tp>> {};

#endif // _LIBCPP_STD_VER >= 17

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___VARIANT_HELPERS_H
