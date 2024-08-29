// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -Wno-unused-value -ast-dump %s \
// RUN:   | FileCheck -strict-whitespace %s

void test_match_structures(int x) {
  x match _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:11> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-WildcardPattern 0x{{[^ ]*}} <col:11>

  x match ? _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:13> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-OptionalPattern 0x{{[^ ]*}} <col:11, col:13>
  // CHECK-NEXT:   `-WildcardPattern 0x{{[^ ]*}} <col:13>

  x match 0;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:11> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:11>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:11> 'int' 0

  x match { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:21> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:13>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:18> 'int' 0

  x match { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:29> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:13>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:18> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:26> 'int' 0

  x match constexpr { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:31> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:23>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:28> 'int' 0

  x match constexpr { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:39> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:23>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:28> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:36> 'int' 0

  x match -> int { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:28> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:20>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:25> 'int' 0

  x match -> auto { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:29> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:21>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:26> 'int' 0

  x match -> decltype(auto) { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:39> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:31>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:36> 'int' 0

  x match -> int { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:36> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:20>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:25> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:33> 'int' 0

  x match -> auto { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:37> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:21>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:26> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:34> 'int' 0

  x match -> decltype(auto) { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:47> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:31>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:36> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:44> 'int' 0

  x match constexpr -> int { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:38> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:30>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:35> 'int' 0

  x match constexpr -> auto { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:39> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:31>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:36> 'int' 0

  x match constexpr -> decltype(auto) { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:49> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:41>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:46> 'int' 0

  x match constexpr -> int { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:46> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:30>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:35> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:43> 'int' 0

  x match constexpr -> auto { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:47> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:31>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:36> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:44> 'int' 0

  x match constexpr -> decltype(auto) { _ if true => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:57> 'int' constexpr
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:41>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:46> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:54> 'int' 0

  x match { ? _ => 0; _ => 1; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:31> 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-
  // CHECK-NEXT: | |-OptionalPattern 0x{{[^ ]*}} <col:13, col:15>
  // CHECK-NEXT: | | `-WildcardPattern 0x{{[^ ]*}} <col:15>
  // CHECK-NEXT: | `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:23>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:28> 'int' 1
}

void test_match_precedence(int* p) {
  /* MatchTestExpr */ {
    // unary is tighter than match
    *p match 0;
    // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:14> 'bool'
    // CHECK-NEXT: |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'int' lvalue prefix '*' cannot overflow
    // CHECK-NEXT: | `-ImplicitCastExpr 0x{{[^ ]*}} <col:6> 'int *' <LValueToRValue>
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
    // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:14>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:14> 'int' 0

    *p match 0 + 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:18> 'int' '+'
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5, col:14> 'int' <IntegralCast>
    // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:5, col:14> 'bool'
    // CHECK-NEXT: |   |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'int' lvalue prefix '*' cannot overflow
    // CHECK-NEXT: |   | `-ImplicitCastExpr 0x{{[^ ]*}} <col:6> 'int *' <LValueToRValue>
    // CHECK-NEXT: |   |   `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
    // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:14>
    // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:14> 'int' 0

    // match binds tighter than bin ops.
    4 + 2 match 0;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:17> 'int' '+'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:9, col:17> 'int' <IntegralCast>
    // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:9, col:17> 'bool'
    // CHECK-NEXT:     |-IntegerLiteral 0x{{[^ ]*}} <col:9> 'int' 2
    // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:17>
    // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 0

    4 * 2 match 0;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:17> 'int' '*'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:9, col:17> 'int' <IntegralCast>
    // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:9, col:17> 'bool'
    // CHECK-NEXT:     |-IntegerLiteral 0x{{[^ ]*}} <col:9> 'int' 2
    // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:17>
    // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 0

    true == 2 match 0;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:21> 'bool' '=='
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5> 'int' <IntegralCast>
    // CHECK-NEXT: | `-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:5> 'bool' true
    // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:13, col:21> 'int' <IntegralCast>
    // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:13, col:21> 'bool'
    // CHECK-NEXT:     |-IntegerLiteral 0x{{[^ ]*}} <col:13> 'int' 2
    // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:21>
    // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:21> 'int' 0

    4 * (2) match 0;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:19> 'int' '*'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:9, col:19> 'int' <IntegralCast>
    // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:9, col:19> 'bool'
    // CHECK-NEXT:     |-ParenExpr 0x{{[^ ]*}} <col:9, col:11> 'int'
    // CHECK-NEXT:     | `-IntegerLiteral 0x{{[^ ]*}} <col:10> 'int' 2
    // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:19>
    // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:19> 'int' 0

    2 match 0 + 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:17> 'int' '+'
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5, col:13> 'int' <IntegralCast>
    // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:5, col:13> 'bool'
    // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:13>
    // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:13> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 1

    2 match 0 * 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:17> 'int' '*'
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5, col:13> 'int' <IntegralCast>
    // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:5, col:13> 'bool'
    // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:13>
    // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:13> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 1

    2 match 0 == 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:18> 'bool' '=='
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5, col:13> 'int' <IntegralCast>
    // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:5, col:13> 'bool'
    // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:13>
    // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:13> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:18> 'int' 1

    (2) match 0 * 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:19> 'int' '*'
    // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:5, col:15> 'int' <IntegralCast>
    // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:5, col:15> 'bool'
    // CHECK-NEXT: |   |-ParenExpr 0x{{[^ ]*}} <col:5, col:7> 'int'
    // CHECK-NEXT: |   | `-IntegerLiteral 0x{{[^ ]*}} <col:6> 'int' 2
    // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:15> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:19> 'int' 1

    // except .* and ->*
    struct S { int i; } s;

    s.*&S::i match 0;
    // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:20> 'bool'
    // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:5, col:12> 'int' lvalue '.*'
    // CHECK-NEXT: | |-DeclRefExpr 0x{{[^ ]*}} <col:5> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:8, col:12> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:9, col:12> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
    // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:20>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0

    &s->*&S::i match 0;
    // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:22> 'bool'
    // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:5, col:14> 'int' lvalue '->*'
    // CHECK-NEXT: | |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'struct S *' prefix '&' cannot overflow
    // CHECK-NEXT: | | `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:10, col:14> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:11, col:14> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
    // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:22>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:22> 'int' 0

    2 match s.*&S::i;
    // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:20> 'bool'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:13, col:20>
    // CHECK-NEXT:   `-BinaryOperator 0x{{[^ ]*}} <col:13, col:20> 'int' lvalue '.*'
    // CHECK-NEXT:     |-DeclRefExpr 0x{{[^ ]*}} <col:13> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT:     `-UnaryOperator 0x{{[^ ]*}} <col:16, col:20> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT:       `-DeclRefExpr 0x{{[^ ]*}} <col:17, col:20> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT:         `-NestedNameSpecifier TypeSpec 'S'

    2 match &s->*&S::i;
    // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:22> 'bool'
    // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: ExpressionPattern 0x{{[^ ]*}} <col:13, col:22>
    // CHECK-NEXT: `-BinaryOperator 0x{{[^ ]*}} <col:13, col:22> 'int' lvalue '->*'
    // CHECK-NEXT:   |-UnaryOperator 0x{{[^ ]*}} <col:13, col:14> 'struct S *' prefix '&' cannot overflow
    // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:14> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT:   `-UnaryOperator 0x{{[^ ]*}} <col:18, col:22> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT:     `-DeclRefExpr 0x{{[^ ]*}} <col:19, col:22> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT:       `-NestedNameSpecifier TypeSpec 'S'
  }

  /* MatchSelectExpr */ {
    // unary is tighter than match
    *p match { _ => 0; };
    // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:24> 'int'
    // CHECK-NEXT: |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'int' lvalue prefix '*' cannot overflow
    // CHECK-NEXT: | `-ImplicitCastExpr 0x{{[^ ]*}} <col:6> 'int *' <LValueToRValue>
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
    // CHECK-NEXT: `-
    // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:16>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:21> 'int' 0

    *p match { _ => 0; } + 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:28> 'int' '+'
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:24> 'int'
    // CHECK-NEXT: | |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'int' lvalue prefix '*' cannot overflow
    // CHECK-NEXT: | | `-ImplicitCastExpr 0x{{[^ ]*}} <col:6> 'int *' <LValueToRValue>
    // CHECK-NEXT: | |   `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:16>
    // CHECK-NEXT: |   `-IntegerLiteral 0x{{[^ ]*}} <col:21> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:28> 'int' 1

    // match binds tighter than bin ops.
    4 + 2 match { _ => 0; };
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:27> 'int' '+'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:9, col:27> 'int'
    // CHECK-NEXT:   |-IntegerLiteral 0x{{[^ ]*}} <col:9> 'int' 2
    // CHECK-NEXT:   `-
    // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:19>
    // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:24> 'int' 0

    4 * 2 match { _ => 0; };
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:27> 'int' '*'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:9, col:27> 'int'
    // CHECK-NEXT:   |-IntegerLiteral 0x{{[^ ]*}} <col:9> 'int' 2
    // CHECK-NEXT:   `-
    // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:19>
    // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:24> 'int' 0

    4 == 2 match { _ => 0; };
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:28> 'bool' '=='
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:10, col:28> 'int'
    // CHECK-NEXT:   |-IntegerLiteral 0x{{[^ ]*}} <col:10> 'int' 2
    // CHECK-NEXT:   `-
    // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:20>
    // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:25> 'int' 0

    4 * (2) match { _ => 0; };
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:29> 'int' '*'
    // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 4
    // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:9, col:29> 'int'
    // CHECK-NEXT:   |-ParenExpr 0x{{[^ ]*}} <col:9, col:11> 'int'
    // CHECK-NEXT:   | `-IntegerLiteral 0x{{[^ ]*}} <col:10> 'int' 2
    // CHECK-NEXT:   `-
    // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:21>
    // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:26> 'int' 0

    2 match { _ => 0; } + 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:27> 'int' '+'
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:23> 'int'
    // CHECK-NEXT: | |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT: |   `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 1

    2 match { _ => 0; } * 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:27> 'int' '*'
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:23> 'int'
    // CHECK-NEXT: | |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT: |   `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 1

    2 match { _ => 0; } == 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:28> 'bool' '=='
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:23> 'int'
    // CHECK-NEXT: | |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT: |   `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:28> 'int' 1

    (2) match { _ => 0; } * 1;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:29> 'int' '*'
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:25> 'int'
    // CHECK-NEXT: | |-ParenExpr 0x{{[^ ]*}} <col:5, col:7> 'int'
    // CHECK-NEXT: | | `-IntegerLiteral 0x{{[^ ]*}} <col:6> 'int' 2
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:17>
    // CHECK-NEXT: |   `-IntegerLiteral 0x{{[^ ]*}} <col:22> 'int' 0
    // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:29> 'int' 1

    // except .* and ->*
    struct S { int i; } s;

    s.*&S::i match { _ => 0; };
    // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:30> 'int'
    // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:5, col:12> 'int' lvalue '.*'
    // CHECK-NEXT: | |-DeclRefExpr 0x{{[^ ]*}} <col:5> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:8, col:12> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:9, col:12> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
    // CHECK-NEXT: `-
    // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:22>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 0

    &s->*&S::i match { _ => 0; };
    // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:32> 'int'
    // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:5, col:14> 'int' lvalue '->*'
    // CHECK-NEXT: | |-UnaryOperator 0x{{[^ ]*}} <col:5, col:6> 'struct S *' prefix '&' cannot overflow
    // CHECK-NEXT: | | `-DeclRefExpr 0x{{[^ ]*}} <col:6> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:10, col:14> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:11, col:14> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
    // CHECK-NEXT: `-
    // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:24>
    // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:29> 'int' 0

    2 match { _ => s; } .* &S::i;
    // CHECK:      ExprWithCleanups 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:32> 'int' xvalue
    // CHECK-NEXT: `-BinaryOperator 0x{{[^ ]*}} <col:5, col:32> 'int' xvalue '.*'
    // CHECK-NEXT:   |-MaterializeTemporaryExpr 0x{{[^ ]*}} <col:5, col:23> 'struct S':'S' xvalue
    // CHECK-NEXT:   | `-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:23> 'struct S':'S'
    // CHECK-NEXT:   |   |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT:   |   `-
    // CHECK-NEXT:   |     |-WildcardPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT:   |     `-DeclRefExpr 0x{{[^ ]*}} <col:20> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT:   `-UnaryOperator 0x{{[^ ]*}} <col:28, col:32> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT:     `-DeclRefExpr 0x{{[^ ]*}} <col:29, col:32> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT:       `-NestedNameSpecifier TypeSpec 'S'

    2 match { _ => &s; } ->* &S::i;
    // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:5, col:34> 'int' lvalue '->*'
    // CHECK-NEXT: |-MatchSelectExpr 0x{{[^ ]*}} <col:5, col:24> 'struct S *'
    // CHECK-NEXT: | |-IntegerLiteral 0x{{[^ ]*}} <col:5> 'int' 2
    // CHECK-NEXT: | `-
    // CHECK-NEXT: |   |-WildcardPattern 0x{{[^ ]*}} <col:15>
    // CHECK-NEXT: |   `-UnaryOperator 0x{{[^ ]*}} <col:20, col:21> 'struct S *' prefix '&' cannot overflow
    // CHECK-NEXT: |     `-DeclRefExpr 0x{{[^ ]*}} <col:21> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
    // CHECK-NEXT: `-UnaryOperator 0x{{[^ ]*}} <col:30, col:34> 'int S::*' prefix '&' cannot overflow
    // CHECK-NEXT:   `-DeclRefExpr 0x{{[^ ]*}} <col:31, col:34> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
    // CHECK-NEXT:     `-NestedNameSpecifier TypeSpec 'S'
  }
}
