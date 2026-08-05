// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wno-unused-value -ast-dump %s \
// RUN:   | FileCheck -strict-whitespace %s

void test_match_dump(int x, int *p) {
  x match case _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:16> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-WildcardPattern 0x{{[^ ]*}} <col:16>

  p match case ? _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:18> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
  // CHECK-NEXT: `-OptionalPattern 0x{{[^ ]*}} <col:16, col:18>
  // CHECK-NEXT:   `-WildcardPattern 0x{{[^ ]*}} <col:18>

  x match { case _ if (true) => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:36> 'int'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:18>
  // CHECK-NEXT:   |-<<<NULL>>>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:24> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:33> 'int' 0

  x match constexpr -> int { case _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:43> 'int' constexpr
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:35>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:40> 'int' 0

  4 + x match { case _ => 0; };
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:30> 'int' '+'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:7, col:30> 'int'
  // CHECK-NEXT:   |-VarDecl 0x{{[^ ]*}} <col:7> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT:   |-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue Var 0x{{[^ ]*}} <col:7> 'int &'
  // CHECK-NEXT:   `-
  // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:22>
  // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:27> 'int' 0

  x match { case int value => value; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:38> 'int'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> <invalid sloc> implicit used 'int &' cinit
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
