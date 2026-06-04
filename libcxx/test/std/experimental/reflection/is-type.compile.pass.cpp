//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03 || c++11 || c++14 || c++17 || c++20 || c++23
// ADDITIONAL_COMPILE_FLAGS: -freflection

// <meta>
// consteval bool is_type(info);

#include <chrono>
#include <meta>
#include <optional>

struct Record {
  int value;
};
enum class Code { Value };
namespace Scope {}
void function();
[[maybe_unused]] constexpr int answer = 42;

using Pointer       = Record*;
using Reference     = const Record&;
using Array         = int[3];
using Function      = void() noexcept;
using MemberPointer = int Record::*;
using Spliced       = [:^^Record:];

static_assert(std::meta::is_type(^^int));
static_assert(std::meta::is_type(^^const int));
static_assert(std::meta::is_type(^^Record));
static_assert(std::meta::is_type(^^Code));
static_assert(std::meta::is_type(^^Pointer));
static_assert(std::meta::is_type(^^Reference));
static_assert(std::meta::is_type(^^Array));
static_assert(std::meta::is_type(^^Function));
static_assert(std::meta::is_type(^^MemberPointer));
static_assert(std::meta::is_type(^^Spliced));

static_assert(!std::meta::is_type(^^answer));
static_assert(!std::meta::is_type(^^std::nullopt));
static_assert(!std::meta::is_type(^^std::chrono::January));
static_assert(!std::meta::is_type(^^Code::Value));
static_assert(!std::meta::is_type(^^Scope));
static_assert(!std::meta::is_type(^^function));
static_assert(!std::meta::is_type(std::meta::info{}));
static_assert(!std::meta::is_type(std::meta::reflect_constant(42)));
static_assert(!std::meta::is_type(std::meta::constant_of(^^answer)));

template <class... Types>
consteval bool dependent_types() {
  return (std::meta::is_type(^^Types) && ...);
}

static_assert(dependent_types<int, Record, Code, Pointer, Reference, Array, Function, MemberPointer>());
