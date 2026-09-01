// RUN: %clang_cc1 -std=c++2d -E %s | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang_cc1 -std=c++2d -freflection -E %s | FileCheck %s --check-prefix=REFLECTION
// RUN: %clang_cc1 -std=c++2d -freflection-latest -E %s | FileCheck %s --check-prefix=REFLECTION
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

#if __has_feature(reflection_latest)
reflection_latest
#else
no_reflection_latest
#endif

#if __has_feature(parameter_reflection)
parameter_reflection
#else
no_parameter_reflection
#endif

#if __has_feature(expansion_statements)
expansion_statements
#else
no_expansion_statements
#endif

#if __has_feature(annotation_attributes)
annotation_attributes
#else
no_annotation_attributes
#endif

// DEFAULT: no_pattern_matching
// DEFAULT: no_reflection
// DEFAULT: no_reflection_latest
// DEFAULT: no_parameter_reflection
// DEFAULT: no_expansion_statements
// DEFAULT: no_annotation_attributes

// REFLECTION: no_pattern_matching
// REFLECTION: reflection
// REFLECTION: reflection_latest
// REFLECTION: parameter_reflection
// REFLECTION: expansion_statements
// REFLECTION: annotation_attributes

// PATTERN: pattern_matching
// PATTERN: reflection
// PATTERN: reflection_latest
// PATTERN: parameter_reflection
// PATTERN: expansion_statements
// PATTERN: annotation_attributes

// PRE-CXX29: error: option '-fpattern-matching' is only supported when compiling in C++29 mode
