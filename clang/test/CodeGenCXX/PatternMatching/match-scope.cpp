// RUN: %clang_cc1 -std=c++2d -fpattern-matching -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM
// RUN: %clang_cc1 -std=c++2d -fpattern-matching -triple x86_64-pc-windows-msvc -emit-llvm -o - %s | FileCheck %s --check-prefix=MS
// RUN: %clang_cc1 -std=c++2d -fpattern-matching -fno-threadsafe-statics -triple x86_64-pc-windows-msvc -emit-llvm -o /dev/null %s

int source();

bool first = match(source(), case int value if ([value] { return value == 42; }()));
bool second = match(source(), case int value if ([value] { return value == 43; }()));

struct Member {
  int source = 42;
  bool matched = match(source, case int value if (value == source));
};

Member member;

namespace constant_init {
int first = match (static int state = 11; state) {
  case int value => value;
};
int second = match (static int state = 22; state) {
  case int value => value;
};
int nested = match (static int state = 33; state) {
  case int value => match (static int state = 44; state) {
    case int value => value;
  };
};
} // namespace constant_init

namespace dynamic_init {
int first = match (static int state = source(); state) {
  case int value => value;
};
int second = match (static int state = source(); state) {
  case int value => value;
};
bool test = match(source(), case int value if (static int state = source(); value == state));
} // namespace dynamic_init

struct StaticMember {
  int value = match (static int state = source(); state) {
    case int &value => ++value;
  };
};

StaticMember static_member;

template <int N> struct TemplatedMember {
  int value = match (static int state = N; state) {
    case int &value => ++value;
  };
};

TemplatedMember<55> templated_member_first;
TemplatedMember<66> templated_member_second;

template <int N> int templated_variable = match (static int state = N; state) {
  case int value => value;
};

int instantiated_variable = templated_variable<77> + templated_variable<88>;

struct DefaultArgument {
  static int first(int value = match (static int state = 99; state) {
    case int value => value;
  }) {
    return value;
  }
  static int second(int value = match (static int state = 100; state) {
    case int value => value;
  }) {
    return value;
  }
};

int default_argument = DefaultArgument::first() + DefaultArgument::second() + DefaultArgument::first();

int function_scope() {
  return match (static int state = source(); state) {
    case int &value => ++value;
  };
}

// ITANIUM-DAG: @_ZZN13constant_initUm1_EvE5state = internal global i32 11
// ITANIUM-DAG: @_ZZN13constant_initUm2_EvE5state = internal global i32 22
// ITANIUM-DAG: @_ZZN13constant_initUm3_EvE5state = internal global i32 33
// ITANIUM-DAG: @_ZZN13constant_initUm3_EvE5state_0 = internal global i32 44
// ITANIUM-DAG: @_ZGVZN12dynamic_initUm1_EvE5state = internal global i64 0
// ITANIUM-DAG: @_ZGVZN12dynamic_initUm2_EvE5state = internal global i64 0
// ITANIUM-DAG: @_ZZN12StaticMember5valueMUm1_EvE5state = linkonce_odr global i32 0
// ITANIUM-DAG: @_ZZN15TemplatedMemberILi55EE5valueMUm1_EvE5state = linkonce_odr global i32 55
// ITANIUM-DAG: @_ZZN15TemplatedMemberILi66EE5valueMUm1_EvE5state = linkonce_odr global i32 66
// ITANIUM-DAG: @_ZZN18templated_variableILi77EEMUm1_EvE5state = linkonce_odr global i32 77
// ITANIUM-DAG: @_ZZN18templated_variableILi88EEMUm1_EvE5state = linkonce_odr global i32 88
// ITANIUM-DAG: @_ZZ14function_scopevE5state = internal global i32 0

// MS-DAG: @"?state@?1?<match_1>@constant_init@@4HA" = internal global i32 11
// MS-DAG: @"?state@?1?<match_2>@constant_init@@4HA" = internal global i32 22
// MS-DAG: @"?state@?1?<match_3>@constant_init@@4HA" = internal global i32 33
// MS-DAG: @"?state@?2?<match_3>@constant_init@@4HA" = internal global i32 44
// MS-DAG: @"?$TSS0@?1?<match_1>@dynamic_init@@4HA" = internal global i32 0
// MS-DAG: @"?$TSS0@?1?<match_2>@dynamic_init@@4HA" = internal global i32 0
// MS-DAG: @"?state@?1?<match_1>@value@StaticMember@@4HA" = linkonce_odr{{.*}} global i32 0
// MS-DAG: @"?$TSS0@?1?<match_1>@value@StaticMember@@4HA" = linkonce_odr global i32 0
