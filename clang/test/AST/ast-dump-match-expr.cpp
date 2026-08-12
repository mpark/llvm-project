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

  x match { case _ if (true) => 0; }; // expected-error {{match expression is not exhaustive; example of a missing case: 0}}
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:36> 'int'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> col:3 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:18>
  // CHECK-NEXT:   |-<<<NULL>>>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:24> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:33> 'int' 0

  x match constexpr -> int { case _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:43> 'int' constexpr
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> col:3 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:35>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:40> 'int' 0

  4 + x match { case _ => 0; };
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:30> 'int' '+'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:7, col:30> 'int'
  // CHECK-NEXT:   |-VarDecl 0x{{[^ ]*}} <col:7> col:7 implicit used 'int &' cinit
  // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT:   |-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue Var 0x{{[^ ]*}} <col:7> 'int &'
  // CHECK-NEXT:   `-
  // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:22>
  // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 0

  x match { case int value => value; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:38> 'int'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> col:3 implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-DeclarationPattern 0x{{[^ ]*}} <col:18, col:22>
  // CHECK-NEXT:   | `-VarDecl 0x{{[^ ]*}} <col:18, col:3> col:22 used value 'int' cinit
  // CHECK-NEXT:   |   `-ImplicitCastExpr 0x{{[^ ]*}} <col:3> 'int' <LValueToRValue>
  // CHECK-NEXT:   |     `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT:   `-ImplicitCastExpr 0x{{[^ ]*}} <col:31> 'int' <LValueToRValue>
  // CHECK-NEXT:     `-ImplicitCastExpr 0x{{[^ ]*}} <col:31> 'int' xvalue <NoOp>
  // CHECK-NEXT:       `-DeclRefExpr 0x{{[^ ]*}} <col:31> 'int' lvalue Var 0x{{[^ ]*}} 'value' 'int'
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

void test_attributed_case_dump(int x) {
  x match { [[likely]] case _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:37> 'int'
  // CHECK:      LikelyAttr 0x{{[^ ]*}} <col:15>
  // CHECK-NEXT: WildcardPattern 0x{{[^ ]*}} <col:29>
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <col:34> 'int' 0
}

void test_default_case_dump(int x) {
  x match { [[unlikely]] default => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:40> 'int'
  // CHECK:      default:
  // CHECK-NEXT: UnlikelyAttr 0x{{[^ ]*}} <col:15>
  // CHECK-NEXT: WildcardPattern 0x{{[^ ]*}} <col:26>
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <col:37> 'int' 0
}
