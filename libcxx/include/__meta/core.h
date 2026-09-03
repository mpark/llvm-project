//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___META_CORE_H
#define _LIBCPP___META_CORE_H

#include <__config>
#include <__type_traits/is_reference.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

#if __has_feature(reflection)

_LIBCPP_BEGIN_NAMESPACE_REFLECTION_V2

// An opaque handle to a reflected entity.
using info = decltype(^^int);

namespace detail {
enum : unsigned {
  // non-exposed metafunctions
  __metafn_get_begin_enumerator_decl_of,
  __metafn_get_get_next_enumerator_decl_of,
  __metafn_get_ith_base_of,
  __metafn_get_ith_template_argument_of,
  __metafn_get_begin_member_decl_of,
  __metafn_get_next_member_decl_of,
  __metafn_is_structural_type,
  __metafn_map_decl_to_entity,

  // P2996 metafunctions
  __metafn_identifier_of,
  __metafn_has_identifier,
  __metafn_operator_of,
  __metafn_source_location_of,
  __metafn_type_of,
  __metafn_parent_of,
  __metafn_underlying_entity_of,
  __metafn_proxied_entity_of,
  __metafn_object_of,
  __metafn_constant_of,
  __metafn_template_of,
  __metafn_substitute,
  __metafn_extract,
  __metafn_is_public,
  __metafn_is_protected,
  __metafn_is_private,
  __metafn_is_virtual,
  __metafn_is_pure_virtual,
  __metafn_is_override,
  __metafn_is_deleted,
  __metafn_is_defaulted,
  __metafn_is_explicit,
  __metafn_is_noexcept,
  __metafn_is_bit_field,
  __metafn_is_enumerator,
  __metafn_is_final,
  __metafn_is_const,
  __metafn_is_volatile,
  __metafn_is_mutable_member,
  __metafn_is_lvalue_reference_qualified,
  __metafn_is_rvalue_reference_qualified,
  __metafn_has_static_storage_duration,
  __metafn_has_thread_storage_duration,
  __metafn_has_automatic_storage_duration,
  __metafn_has_internal_linkage,
  __metafn_has_module_linkage,
  __metafn_has_external_linkage,
  __metafn_has_linkage,
  __metafn_is_class_member,
  __metafn_is_namespace_member,
  __metafn_is_nonstatic_data_member,
  __metafn_is_static_member,
  __metafn_is_base,
  __metafn_is_data_member_spec,
  __metafn_is_namespace,
  __metafn_is_function,
  __metafn_is_variable,
  __metafn_is_type,
  __metafn_is_alias,
  __metafn_is_entity_proxy,
  __metafn_is_complete_type,
  __metafn_has_complete_definition,
  __metafn_is_enumerable_type,
  __metafn_is_template,
  __metafn_is_function_template,
  __metafn_is_variable_template,
  __metafn_is_class_template,
  __metafn_is_alias_template,
  __metafn_is_conversion_function_template,
  __metafn_is_operator_function_template,
  __metafn_is_literal_operator_template,
  __metafn_is_constructor_template,
  __metafn_is_concept,
  __metafn_is_structured_binding,
  __metafn_is_value,
  __metafn_is_object,
  __metafn_has_template_arguments,
  __metafn_has_default_member_initializer,
  __metafn_is_conversion_function,
  __metafn_is_operator_function,
  __metafn_is_literal_operator,
  __metafn_is_constructor,
  __metafn_is_default_constructor,
  __metafn_is_copy_constructor,
  __metafn_is_move_constructor,
  __metafn_is_assignment,
  __metafn_is_copy_assignment,
  __metafn_is_move_assignment,
  __metafn_is_destructor,
  __metafn_is_special_member_function,
  __metafn_is_user_provided,
  __metafn_is_user_declared,
  __metafn_reflect_result,
  __metafn_data_member_spec,
  __metafn_define_aggregate,
  __metafn_offset_of,
  __metafn_size_of,
  __metafn_bit_offset_of,
  __metafn_bit_size_of,
  __metafn_alignment_of,

  // P3096 parameter reflection metafunctions
  __metafn_get_ith_parameter_of,
  __metafn_has_ellipsis_parameter,
  __metafn_has_default_argument,
  __metafn_is_explicit_object_parameter,
  __metafn_is_function_parameter,
  __metafn_return_type_of,
  __metafn_variable_of,

  // P3394 annotation metafunctions
  __metafn_get_ith_annotation_of,
  __metafn_is_annotation,
  __metafn_annotate,

  // P3493 accessibility metafunctions
  __metafn_access_context,
  __metafn_is_accessible,

  // Other bespoke functions (not proposed at this time)
  __metafn_is_access_specified,
  __metafn_reflect_invoke,
};
} // namespace detail

// Returns a reflection of the value evaluated from the reflected entity.
consteval auto constant_of(info __reflection) -> info {
  return __metafunction(detail::__metafn_constant_of, __reflection);
}

// Returns whether the reflected entity is a type.
consteval auto is_type(info __reflection) -> bool { return __metafunction(detail::__metafn_is_type, __reflection); }

consteval auto is_structural_type(info __reflection) -> bool {
  return __metafunction(detail::__metafn_is_structural_type, __reflection);
}

// Returns a reflection of the value held by the provided argument.
template <typename _Tp>
  requires(!is_reference_v<_Tp> && is_structural_type(^^_Tp))
consteval auto reflect_constant(_Tp __value) -> info {
  return __metafunction(detail::__metafn_reflect_result, ^^_Tp, __value);
}

_LIBCPP_END_NAMESPACE_REFLECTION_V2

#endif // __has_feature(reflection)

#endif // _LIBCPP___META_CORE_H
