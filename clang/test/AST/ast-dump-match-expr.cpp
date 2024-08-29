// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -Wno-unused-value -ast-dump %s \
// RUN:   | FileCheck -strict-whitespace %s

void test_match_test_precedence(int* p) {
  // unary is tighter than match
  *p match 0;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:12> 'bool'
  // CHECK-NEXT: |-UnaryOperator 0x{{[^ ]*}} <col:3, col:4> 'int' lvalue prefix '*' cannot overflow
  // CHECK-NEXT: | `-ImplicitCastExpr 0x{{[^ ]*}} <col:4> 'int *' <LValueToRValue>
  // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:4> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:12>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:12> 'int' 0

  *p match 0 + 1;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:16> 'int' '+'
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3, col:12> 'int' <IntegralCast>
  // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:3, col:12> 'bool'
  // CHECK-NEXT: |   |-UnaryOperator 0x{{[^ ]*}} <col:3, col:4> 'int' lvalue prefix '*' cannot overflow
  // CHECK-NEXT: |   | `-ImplicitCastExpr 0x{{[^ ]*}} <col:4> 'int *' <LValueToRValue>
  // CHECK-NEXT: |   |   `-DeclRefExpr 0x{{[^ ]*}} <col:4> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
  // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:12>
  // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:12> 'int' 0

  // match binds tighter than bin ops.
  4 + 2 match 0;
  // CHECK:      |-BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:15> 'int' '+'
  // CHECK-NEXT: | |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: | `-ImplicitCastExpr 0x{{[^ ]*}} <col:7, col:15> 'int' <IntegralCast>
  // CHECK-NEXT: |   `-MatchTestExpr 0x{{[^ ]*}} <col:7, col:15> 'bool'
  // CHECK-NEXT: |     |-IntegerLiteral 0x{{[^ ]*}} <col:7> 'int' 2
  // CHECK-NEXT: |     `-ExpressionPattern 0x{{[^ ]*}} <col:15>
  // CHECK-NEXT: |       `-IntegerLiteral 0x{{[^ ]*}} <col:15> 'int' 0

  4 * 2 match 0;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:15> 'int' '*'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:7, col:15> 'int' <IntegralCast>
  // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:7, col:15> 'bool'
  // CHECK-NEXT:     |-IntegerLiteral 0x{{[^ ]*}} <col:7> 'int' 2
  // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:15>
  // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:15> 'int' 0

  true == 2 match 0;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:19> 'bool' '=='
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3> 'int' <IntegralCast>
  // CHECK-NEXT: | `-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:3> 'bool' true
  // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:11, col:19> 'int' <IntegralCast>
  // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:11, col:19> 'bool'
  // CHECK-NEXT:     |-IntegerLiteral 0x{{[^ ]*}} <col:11> 'int' 2
  // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:19>
  // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:19> 'int' 0

  4 * (2) match 0;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:17> 'int' '*'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-ImplicitCastExpr 0x{{[^ ]*}} <col:7, col:17> 'int' <IntegralCast>
  // CHECK-NEXT:   `-MatchTestExpr 0x{{[^ ]*}} <col:7, col:17> 'bool'
  // CHECK-NEXT:     |-ParenExpr 0x{{[^ ]*}} <col:7, col:9> 'int'
  // CHECK-NEXT:     | `-IntegerLiteral 0x{{[^ ]*}} <col:8> 'int' 2
  // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:17>
  // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 0

  2 match 0 + 1;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:15> 'int' '+'
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3, col:11> 'int' <IntegralCast>
  // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:3, col:11> 'bool'
  // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 2
  // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:11>
  // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:11> 'int' 0
  // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:15> 'int' 1

  2 match 0 * 1;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:15> 'int' '*'
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3, col:11> 'int' <IntegralCast>
  // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:3, col:11> 'bool'
  // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 2
  // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:11>
  // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:11> 'int' 0
  // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:15> 'int' 1

  2 match 0 == 1;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:16> 'bool' '=='
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3, col:11> 'int' <IntegralCast>
  // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:3, col:11> 'bool'
  // CHECK-NEXT: |   |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 2
  // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:11>
  // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:11> 'int' 0
  // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:16> 'int' 1

  (2) match 0 * 1;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:17> 'int' '*'
  // CHECK-NEXT: |-ImplicitCastExpr 0x{{[^ ]*}} <col:3, col:13> 'int' <IntegralCast>
  // CHECK-NEXT: | `-MatchTestExpr 0x{{[^ ]*}} <col:3, col:13> 'bool'
  // CHECK-NEXT: |   |-ParenExpr 0x{{[^ ]*}} <col:3, col:5> 'int'
  // CHECK-NEXT: |   | `-IntegerLiteral 0x{{[^ ]*}} <col:4> 'int' 2
  // CHECK-NEXT: |   `-ExpressionPattern 0x{{[^ ]*}} <col:13>
  // CHECK-NEXT: |     `-IntegerLiteral 0x{{[^ ]*}} <col:13> 'int' 0
  // CHECK-NEXT: `-IntegerLiteral 0x{{[^ ]*}} <col:17> 'int' 1

  // except .* and ->*
  struct S { int i; } s;

  s.*&S::i match 0;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:18> 'bool'
  // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:3, col:10> 'int' lvalue '.*'
  // CHECK-NEXT: | |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
  // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:6, col:10> 'int S::*' prefix '&' cannot overflow
  // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:7, col:10> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
  // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:18>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:18> 'int' 0

  &s->*&S::i match 0;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:20> 'bool'
  // CHECK-NEXT: |-BinaryOperator 0x{{[^ ]*}} <col:3, col:12> 'int' lvalue '->*'
  // CHECK-NEXT: | |-UnaryOperator 0x{{[^ ]*}} <col:3, col:4> 'struct S *' prefix '&' cannot overflow
  // CHECK-NEXT: | | `-DeclRefExpr 0x{{[^ ]*}} <col:4> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
  // CHECK-NEXT: | `-UnaryOperator 0x{{[^ ]*}} <col:8, col:12> 'int S::*' prefix '&' cannot overflow
  // CHECK-NEXT: |   `-DeclRefExpr 0x{{[^ ]*}} <col:9, col:12> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
  // CHECK-NEXT: |     `-NestedNameSpecifier TypeSpec 'S'
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:20>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0

  2 match s.*&S::i;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:18> 'bool'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 2
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:11, col:18>
  // CHECK-NEXT:   `-BinaryOperator 0x{{[^ ]*}} <col:11, col:18> 'int' lvalue '.*'
  // CHECK-NEXT:     |-DeclRefExpr 0x{{[^ ]*}} <col:11> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
  // CHECK-NEXT:     `-UnaryOperator 0x{{[^ ]*}} <col:14, col:18> 'int S::*' prefix '&' cannot overflow
  // CHECK-NEXT:       `-DeclRefExpr 0x{{[^ ]*}} <col:15, col:18> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
  // CHECK-NEXT:         `-NestedNameSpecifier TypeSpec 'S'

  2 match &s->*&S::i;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:20> 'bool'
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 2
  // CHECK-NEXT: ExpressionPattern 0x{{[^ ]*}} <col:11, col:20>
  // CHECK-NEXT: `-BinaryOperator 0x{{[^ ]*}} <col:11, col:20> 'int' lvalue '->*'
  // CHECK-NEXT:   |-UnaryOperator 0x{{[^ ]*}} <col:11, col:12> 'struct S *' prefix '&' cannot overflow
  // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:12> 'struct S':'S' lvalue Var 0x{{[^ ]*}} 's' 'struct S':'S'
  // CHECK-NEXT:   `-UnaryOperator 0x{{[^ ]*}} <col:16, col:20> 'int S::*' prefix '&' cannot overflow
  // CHECK-NEXT:     `-DeclRefExpr 0x{{[^ ]*}} <col:17, col:20> 'int' lvalue Field 0x{{[^ ]*}} 'i' 'int'
  // CHECK-NEXT:       `-NestedNameSpecifier TypeSpec 'S'
}
