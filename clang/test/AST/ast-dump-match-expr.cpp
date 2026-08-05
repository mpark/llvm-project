// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching \
// RUN:   -Wno-unused-value -ast-dump %s \
// RUN:   | FileCheck -strict-whitespace %s

void test_match_dump(int x, int *p) {
  x match _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:11> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: `-WildcardPattern 0x{{[^ ]*}} <col:11>

  p match ? _;
  // CHECK:      MatchTestExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:13> 'bool'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int *' lvalue ParmVar 0x{{[^ ]*}} 'p' 'int *'
  // CHECK-NEXT: `-OptionalPattern 0x{{[^ ]*}} <col:11, col:13>
  // CHECK-NEXT:   `-WildcardPattern 0x{{[^ ]*}} <col:13>

  x match { _ if (true) => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:31> 'int'
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:13>
  // CHECK-NEXT:   |-<<<NULL>>>
  // CHECK-NEXT:   |-CXXBoolLiteralExpr 0x{{[^ ]*}} <col:19> 'bool' true
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:28> 'int' 0

  x match constexpr -> int { _ => 0; };
  // CHECK:      MatchSelectExpr 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:38> 'int' constexpr
  // CHECK-NEXT: |-VarDecl 0x{{[^ ]*}} <col:3> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT: | `-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT: |-DeclRefExpr 0x{{[^ ]*}} <col:3> 'int' lvalue Var 0x{{[^ ]*}} <col:3> 'int &'
  // CHECK-NEXT: `-
  // CHECK-NEXT:   |-WildcardPattern 0x{{[^ ]*}} <col:30>
  // CHECK-NEXT:   `-IntegerLiteral 0x{{[^ ]*}} <col:35> 'int' 0

  4 + x match { _ => 0; };
  // CHECK:      BinaryOperator 0x{{[^ ]*}} <line:[[@LINE-1]]:3, col:25> 'int' '+'
  // CHECK-NEXT: |-IntegerLiteral 0x{{[^ ]*}} <col:3> 'int' 4
  // CHECK-NEXT: `-MatchSelectExpr 0x{{[^ ]*}} <col:7, col:25> 'int'
  // CHECK-NEXT:   |-VarDecl 0x{{[^ ]*}} <col:7> <invalid sloc> implicit used 'int &' cinit
  // CHECK-NEXT:   | `-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue ParmVar 0x{{[^ ]*}} 'x' 'int'
  // CHECK-NEXT:   |-DeclRefExpr 0x{{[^ ]*}} <col:7> 'int' lvalue Var 0x{{[^ ]*}} <col:7> 'int &'
  // CHECK-NEXT:   `-
  // CHECK-NEXT:     |-WildcardPattern 0x{{[^ ]*}} <col:17>
  // CHECK-NEXT:     `-IntegerLiteral 0x{{[^ ]*}} <col:22> 'int' 0
}
