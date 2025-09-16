// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t

//        Module: | Flag & Used | Flag/Unused | NoFlag/Used | NoFlag/Unused
// ---------------+-------------+-------------+-------------+---------------
// Source: Flag   |   Accepted  |   Accepted  |  Rejected   |    Accepted
// Source: NoFlag |   Rejected  |   Accepted  |  Accepted   |    Accepted

// Module 1: Flag & Used
// RUN: %clang_cc1 -std=c++20 -emit-header-unit -xc++-user-header \
// RUN:   -stack-protector 1 %t/used.h -o %t/flag_and_used.pcm

// Module 2: Flag & Unused
// RUN: %clang_cc1 -std=c++20 -emit-header-unit -xc++-user-header \
// RUN:   -stack-protector 1 %t/unused.h -o %t/flag_and_unused.pcm

// Module 3: NoFlag & Used
// RUN: %clang_cc1 -std=c++20 -emit-header-unit -xc++-user-header \
// RUN:   %t/used.h -o %t/no_flag_and_used.pcm

// Module 4: NoFlag & Unused
// RUN: %clang_cc1 -std=c++20 -emit-header-unit -xc++-user-header \
// RUN:   %t/unused.h -o %t/no_flag_and_unused.pcm

// Tests for Source: Flag

//   Test 1 / Module 1: Flag & Used / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_used.cpp \
//   RUN:  -Wno-experimental-header-units -stack-protector 1 \
//   RUN:  -fmodule-file=%t/flag_and_used.pcm

//   Test 2 / Module 2: Flag & Unused / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_unused.cpp \
//   RUN:  -Wno-experimental-header-units -stack-protector 1   \
//   RUN:  -fmodule-file=%t/flag_and_unused.pcm

//   Test 3 / Module 3: NoFlag & Used / Rejected
//   RUN: not %clang_cc1 -std=c++20 -emit-obj %t/import_used.cpp \
//   RUN:  -Wno-experimental-header-units -stack-protector 1     \
//   RUN:  -fmodule-file=%t/no_flag_and_used.pcm 2>&1 | FileCheck -check-prefix=CHECK-NOFLAG-AND-USED %s

//   Test 4 / Module 4: NoFlag & Unused / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_unused.cpp \
//   RUN:  -Wno-experimental-header-units -stack-protector 1   \
//   RUN:  -fmodule-file=%t/no_flag_and_unused.pcm

// CHECK-NOFLAG-AND-USED: error: stack protector mode differs in precompiled file '{{.*[/|\\\\]}}no_flag_and_used.pcm' vs. current file
// CHECK-NOFLAG-AND-USED-NEXT: error: module file {{.*[/|\\\\]}}no_flag_and_used.pcm cannot be loaded due to a configuration mismatch with the current compilation

// Tests for Source: NoFlag

//   Test 5 / Module 1: Flag & Used / Rejected
//   RUN: not %clang_cc1 -std=c++20 -emit-obj %t/import_used.cpp \
//   RUN:  -Wno-experimental-header-units                        \
//   RUN:  -fmodule-file=%t/flag_and_used.pcm 2>&1 | FileCheck -check-prefix=CHECK-FLAG-AND-USED %s

//   Test 6 / Module 2: Flag & Unused / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_unused.cpp \
//   RUN:  -Wno-experimental-header-units                      \
//   RUN:  -fmodule-file=%t/flag_and_unused.pcm

//   Test 7 / Module 3: NoFlag & Used / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_used.cpp \
//   RUN:  -Wno-experimental-header-units                    \
//   RUN:  -fmodule-file=%t/no_flag_and_used.pcm

//   Test 8 / Module 4: NoFlag & Unused / Accepted
//   RUN: %clang_cc1 -std=c++20 -emit-obj %t/import_unused.cpp \
//   RUN:  -Wno-experimental-header-units                      \
//   RUN:  -fmodule-file=%t/no_flag_and_unused.pcm

// CHECK-FLAG-AND-USED: error: stack protector mode differs in precompiled file '{{.*[/|\\\\]}}flag_and_used.pcm' vs. current file
// CHECK-FLAG-AND-USED-NEXT: error: module file {{.*[/|\\\\]}}flag_and_used.pcm cannot be loaded due to a configuration mismatch with the current compilation

//--- used.h
#ifdef __SSP__
inline void foo() {}
#else
inline void bar() {}
#endif

//--- unused.h
inline void foo() {}

//--- import_used.cpp
import "used.h";

void f() {
#ifdef __SSP__
  foo();
#else
  bar();
#endif
}

//--- import_unused.cpp
import "unused.h";

void f() {
  foo();
}
