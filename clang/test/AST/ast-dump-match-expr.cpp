// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wno-unused-value -ast-dump -verify %s \
// RUN:   | FileCheck -strict-whitespace %s

void test_match_dump(int x, int *p) {
  x match case _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:16> 'bool'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> col:3 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-WildcardPattern 0x{{[^ ]*}} <col:16>

  match (x) { case _ if (true) => 0; } // expected-error {{match expression is not exhaustive; example of a missing case: 0}}
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:38> 'void' statement
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:10> col:10 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:10> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:10> 'int' lvalue Var 0x{{[^ ]*}} <col:10> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:20>
  // CHECK-NEXT:   |-<<<NULL>>>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:26> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:35> 'int' 0

  (void)match constexpr (x) -> int { case _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <col:9, col:51> 'int' constexpr
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:26> col:26 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:26> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:26> 'int' lvalue Var 0x{{[^ ]*}} <col:26> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:43>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:48> 'int' 0

  4 + match (x) { case _ => 0; };
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:32> 'int' '+'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:7, col:32> 'int'
  // CHECK-NEXT:   |-VarDecl 0x{{[^ ]*}} <col:14> col:14 implicit used 'int &' cinit
  // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:14> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT:   |-DeclRefExpr 0x{{[^ ]*}} <col:14> 'int' lvalue Var 0x{{[^ ]*}} <col:14> 'int &'
  // CHECK-NEXT:   `-
  // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:24>
  // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:29> 'int' 0

  match (x) { case int value => value; }
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:40> 'void' statement
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:10> col:10 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:10> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:10> 'int' lvalue Var 0x{{[^ ]*}} <col:10> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-DeclarationPattern 0x{{[^ ]*}} <col:20, col:24>
  // CHECK-NEXT:   | `-VarDecl 0x{{[^ ]*}} <col:20, col:10> col:24 used value 'int' cinit
  // CHECK-NEXT:   |   `-ImplicitCastExpr 0x{{[^ ]*}} <col:10> 'int' <LValueToRValue>
  // CHECK-NEXT:   |     `-DeclRefExpr 0x{{[^ ]*}} <col:10> 'int' lvalue Var 0x{{[^ ]*}} <col:10> 'int &'
  // CHECK-NEXT:   `-DeclRefExpr 0x{{[^ ]*}} <col:33> 'int' lvalue Var 0x{{[^ ]*}} 'value' 'int'
}

void test_case_condition_dump(int x) {
  if (case int value = x && value > 0)
    (void)value;
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-2]]:7, col:37> 'bool' '&&'
  // CHECK-NEXT: |-CaseConditionExpr 0x{{[^ ]*}} <col:7, col:24> 'bool'
  // CHECK:      | `-DeclarationPattern 0x{{[^ ]*}} <col:12, col:16>
  // CHECK:      `-BinaryOperator 0x{{[^ ]*}} <col:29, col:37> 'bool' '>'
}

void test_type_pattern_dump(int x) {
  x match case int;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:16> 'bool'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> col:3 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-TypePattern 0x{{[^ ]*}} <col:16>
  // CHECK-NEXT:   `-BuiltinType 0x{{[^ ]*}} 'int'
}

constexpr int pattern_value = 1;

void test_declaration_expression_disambiguation_dump(int x) {
  match (x) { case int(pattern_value) => 0; case _ => 1; }
  // CHECK:      ExpressionPattern 0x{{[^ ]*}} <col:20, col:37>
  // CHECK-NEXT: `-CXXFunctionalCastExpr 0x{{[^ ]*}} <col:20, col:37> 'int' functional cast to int <NoOp>

  match (x) { case auto(pattern_value) => 0; case _ => 1; }
  // CHECK:      ExpressionPattern 0x{{[^ ]*}} <col:20, col:38>
  // CHECK-NEXT: `-CXXFunctionalCastExpr 0x{{[^ ]*}} <col:20, col:38> 'int' functional cast to auto <NoOp>

  match (x) { case int() => 0; case _ => 1; }
  // CHECK:      ExpressionPattern 0x{{[^ ]*}} <col:20, col:24>
  // CHECK-NEXT: `-CXXScalarValueInitExpr 0x{{[^ ]*}} <col:20, col:24> 'int'

  match (x) { case int named => named; }
  // CHECK:      DeclarationPattern 0x{{[^ ]*}} <col:20, col:24>
}

void test_or_pattern_dump(int x) {
  match (x) { case 0 || 1 || 2 => 0; case _ => 1; }
  // CHECK:      OrPattern 0x{{[^ ]*}} <col:20, col:30>
  // CHECK-NEXT: |-ExpressionPattern 0x{{[^ ]*}} <col:20>
  // CHECK-NEXT: | `-IntegerLiteral 0x{{[^ ]*}} <col:20> 'int' 0
  // CHECK-NEXT: |-ExpressionPattern 0x{{[^ ]*}} <col:25>
  // CHECK-NEXT: | `-IntegerLiteral 0x{{[^ ]*}} <col:25> 'int' 1
  // CHECK-NEXT: `-ExpressionPattern 0x{{[^ ]*}} <col:30>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:30> 'int' 2
}

void test_paren_pattern_dump(int x) {
  match (x) { case ((0 || 1)) => 0; case _ => 1; }
  // CHECK:      ParenPattern 0x{{[^ ]*}} <col:20, col:29>
  // CHECK-NEXT: `-ParenPattern 0x{{[^ ]*}} <col:21, col:28>
  // CHECK-NEXT:   `-OrPattern 0x{{[^ ]*}} <col:22, col:27>
  // CHECK-NEXT:     |-ExpressionPattern 0x{{[^ ]*}} <col:22>
  // CHECK-NEXT:     | `-IntegerLiteral 0x{{[^ ]*}} <col:22> 'int' 0
  // CHECK-NEXT:     `-ExpressionPattern 0x{{[^ ]*}} <col:27>
  // CHECK-NEXT:       `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 1
}

void test_attributed_case_dump(int x) {
  match (x) { [[likely]] case _ => 0; }
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:39> 'void' statement
  // CHECK:      LikelyAttr 0x{{[^ ]*}} <col:17>
  // CHECK-NEXT: WildcardPattern 0x{{[^ ]*}} <col:31>
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <col:36> 'int' 0
}
