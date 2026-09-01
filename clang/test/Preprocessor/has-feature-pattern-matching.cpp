// RUN: %clang_cc1 -std=c++2d -E %s | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang_cc1 -std=c++2d -freflection -E %s | FileCheck %s --check-prefix=REFLECTION
// RUN: %clang_cc1 -std=c++2d -fpattern-matching -E %s | FileCheck %s --check-prefix=PATTERN
// RUN: not %clang_cc1 -std=c++2c -fpattern-matching -E %s 2>&1 | FileCheck %s --check-prefix=PRE-CXX29

#if __has_feature(pattern_matching)
pattern_matching
#else
no_pattern_matching
#endif

#if __has_feature(reflection)
reflection
#else
no_reflection
#endif

// DEFAULT: no_pattern_matching
// DEFAULT: no_reflection

// REFLECTION: no_pattern_matching
// REFLECTION: reflection

// PATTERN: pattern_matching
// PATTERN: reflection

// PRE-CXX29: error: option '-fpattern-matching' is only supported when compiling in C++29 mode
