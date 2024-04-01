// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -ast-dump %s | FileCheck %s

void TestInspect(int a, int b) {
  inspect(3) {
    __ => {};
  };
  // CHECK: InspectExpr 0x{{[^ ]*}} <line:[[@LINE-3]]:3, line:[[@LINE-1]]:3> 'void' has_implicit_result_type
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <line:[[@LINE-4]]:11> 'int' 3
  // CHECK-NEXT: CompoundStmt 0x{{[^ ]*}} <col:14, line:[[@LINE-3]]:3>
  // CHECK-NEXT: WildcardPatternStmt 0x{{[^ ]*}} <line:[[@LINE-5]]:5, col:12>
  // CHECK-NEXT: CompoundStmt

  inspect(a) {
    __ if (b>0) => {};
  };
  // CHECK: InspectExpr 0x{{[^ ]*}} <line:[[@LINE-3]]:3, line:[[@LINE-1]]:3> 'void' has_implicit_result_type
  // CHECK: WildcardPatternStmt 0x{{[^ ]*}} <line:[[@LINE-3]]:5, col:21> has_guard
  // CHECK-NEXT: CompoundStmt
  // CHECK-NEXT: BinaryOperator 0x{{[^ ]*}} <col:12, col:14> 'bool' '>'

  inspect(3) -> void {
    __ => {};
  };
  // CHECK: InspectExpr 0x{{[^ ]*}} <line:[[@LINE-3]]:3, line:[[@LINE-1]]:3> 'void' has_explicit_result_type
  // CHECK-NEXT: IntegerLiteral 0x{{[^ ]*}} <line:[[@LINE-4]]:11> 'int' 3
  // CHECK-NEXT: CompoundStmt
  // CHECK-NEXT: WildcardPatternStmt 0x{{[^ ]*}} <line:[[@LINE-5]]:5, col:12>
  // CHECK-NEXT: CompoundStmt

  int x = 3;
  int w = inspect(x) -> int {
    y if (y>0) => y++;
  };
  // CHECK: InspectExpr {{.*}}'int' has_explicit_result_type
  // CHECK: IdentifierPatternStmt 0x{{[^ ]*}} <line:[[@LINE-3]]:5, col:20> has_guard
  // CHECK-NEXT: UnaryOperator 0x{{.*}} <col:19, col:20> 'int' postfix '++'
  // CHECK-NEXT: DeclRefExpr 0x{{[^ ]*}} <col:19> 'int' lvalue Var 0x{{[^ ]*}} 'y' 'int &&'
  // CHECK-NEXT: DeclStmt 0x{{[^ ]*}}
  // CHECK-NEXT: VarDecl 0x{{.*}} used y 'int &&' auto cinit
  // CHECK: BinaryOperator {{.*}} 'bool' '>'
  // CHECK: DeclRefExpr {{.*}} <col:11> 'int' lvalue Var {{.*}} 'y' 'int &&'

  int v = inspect(x) -> int {
    7 => 4;
  };
  // CHECK: ExpressionPatternStmt {{.*}} <line:[[@LINE-2]]:5, col:10>
  // CHECK-NEXT: IntegerLiteral {{.*}} <col:10> 'int' 4
  // CHECK-NEXT: BinaryOperator {{.*}} <line:[[@LINE-5]]:19, line:[[@LINE-4]]:5>  'bool' '=='
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <line:[[@LINE-6]]:19> 'int' <LValueToRValue>
  // CHECK-NEXT: DeclRefExpr {{.*}} <col:19> 'int' lvalue Var {{.*}} 'x' 'int'
  // CHECK-NEXT: ConstantExpr {{.*}} 'int'
  // CHECK: IntegerLiteral {{.*}} 'int' 7

  inspect(x) {
    case 7 =>;
  };
  // CHECK: ExpressionPatternStmt {{.*}} <line:[[@LINE-2]]:10, col:14> has_case

  enum class Color { Red, Green, Blue };
  enum Color2 { Red, Green, Blue };

  inspect (x) {
    (int)Color::Red => {}
    case Green => {}
    8 => {}
    Green if(x>0) => {} // should be parsed as identifier pattern
    __ => {}
  };
  // CHECK: InspectExpr {{.*}} 'void' has_implicit_result_type
  // CHECK: ExpressionPatternStmt
  // CHECK: ExpressionPatternStmt
  // CHECK: ExpressionPatternStmt
  // CHECK: IdentifierPatternStmt
  // CHECK: WildcardPatternStmt

  struct s {
    int a;
    int b;
  };
  s cond{1,2};
  inspect (cond) {
    [1, 2] => { cond.a++; };
  };
  // CHECK: InspectExpr {{.*}} 'void' has_implicit_result_type
  // CHECK: StructuredBindingPatternStmt {{.*}}
  // CHECK: CompoundStmt {{.*}} <col:15, col:27>
  // CHECK: DeclStmt
  // CHECK: `-DecompositionDecl {{.*}} used 's &' cinit
  // CHECK:   |-BindingDecl {{.*}} <col:12> col:12 __pat_0_0 'int'
  // CHECK:     `-MemberExpr {{.*}} <col:12> 'int' lvalue .a
  // CHECK:   `-BindingDecl {{.*}} <col:12> col:12 __pat_0_1 'int'
  // CHECK:      `-MemberExpr {{.*}} <col:12> 'int' lvalue .b
  // CHECK: BinaryOperator {{.*}} 'bool' '&&'
  // CHECK: |-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:   |-ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
  // CHECK:   | `-MemberExpr {{.*}} 'int' lvalue .a
  // CHECK:   |   `-DeclRefExpr {{.*}} 's' lvalue Decomposition {{.*}} '' 's &'
  // CHECK:   `-ConstantExpr {{.*}} 'int'
  // CHECK:     `-IntegerLiteral {{.*}} 'int' 1
  // CHECK: `-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:   |-ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
  // CHECK:   | `-MemberExpr {{.*}} 'int' lvalue .b
  // CHECK:   |   `-DeclRefExpr {{.*}} 's' lvalue Decomposition {{.*}} '' 's &'
  // CHECK:   `-ConstantExpr {{.*}} 'int'
  // CHECK:     `-IntegerLiteral {{.*}} 'int' 2

  struct color_pack {
    Color2 c1, c2;
  };
  color_pack cp{Red, Blue};
  inspect (cp) {
    [case Red, case Green] =>;
    [Color2::Red, Color2::Blue] =>;
    [Green, case Color2::Red] =>;
  };

  // CHECK: InspectExpr {{.*}} 'void' has_implicit_result_type
  // CHECK: StructuredBindingPatternStmt
  // CHECK: `-BinaryOperator {{.*}} 'bool' '&&'
  // CHECK:   |-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     `-MemberExpr {{.*}} 'Color2' lvalue .c1
  // CHECK:       `-DeclRefExpr {{.*}} <col:11> 'Color2' EnumConstant {{.*}} 'Red' 'Color2'
  // CHECK:   `-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     `-MemberExpr {{.*}} <col:12> 'Color2' lvalue .c2
  // CHECK:       `-DeclRefExpr {{.*}} <col:21> 'Color2' EnumConstant {{.*}} 'Green' 'Color2'
  // CHECK: StructuredBindingPatternStmt
  // CHECK: `-BinaryOperator {{.*}} 'bool' '&&'
  // CHECK:   |-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     `-MemberExpr {{.*}} <col:12> 'Color2' lvalue .c1
  // CHECK:       `-DeclRefExpr {{.*}} <col:6, col:14> 'Color2' EnumConstant {{.*}} 'Red' 'Color2'
  // CHECK:   `-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     `-MemberExpr {{.*}} <col:12> 'Color2' lvalue .c2
  // CHECK:       `-DeclRefExpr {{.*}} <col:19, col:27> 'Color2' EnumConstant {{.*}} 'Blue' 'Color2'
  // CHECK: StructuredBindingPatternStmt
  // CHECK: DeclStmt
  // CHECK: `-DecompositionDecl {{.*}} 'color_pack &' cinit
  // CHECK:   |-BindingDecl {{.*}} col:6 Green 'Color2'
  // CHECK:   | `-MemberExpr {{.*}} <col:6> 'Color2' lvalue .c1
  // CHECK:   `-BindingDecl {{.*}} col:12 __pat_3_1 'Color2'
  // CHECK:     `-MemberExpr {{.*}} <col:12> 'Color2' lvalue .c2
  // CHECK: `-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:   `-MemberExpr {{.*}} <col:12> 'Color2' lvalue .c2
  // CHECK:     `-DeclRefExpr {{.*}} <col:12> 'color_pack' lvalue Decomposition {{.*}} '' 'color_pack &'
  // CHECK:   `-ConstantExpr {{.*}} <col:18, col:26> 'Color2'

  int array[2] = {2,1};
  inspect (array) {
    [1,2] =>;
  };
  // CHECK: InspectExpr
  // CHECK: StructuredBindingPatternStmt
  // CHECK: BinaryOperator {{.*}} 'bool' '&&'
  // CHECK:   BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     ArraySubscriptExpr {{.*}} <col:12> 'int' lvalue
  // CHECK:       IntegerLiteral {{.*}} <col:12> 'int' 0
  // CHECK:   BinaryOperator
  // CHECK:     ArraySubscriptExpr {{.*}} <col:12> 'int' lvalue
  // CHECK:       IntegerLiteral {{.*}} <col:12> 'int' 1

  struct insn_type {
    unsigned opc : 16, imm : 16;
  };
  insn_type insn;
  inspect(insn) {
    [o, i] if (o+i < 12) => { o++; };
  };
  // CHECK: InspectExpr
  // CHECK: StructuredBindingPatternStmt
  // CHECK: |-CompoundStmt {{.*}} <col:29, col:36>
  // CHECK: | `-UnaryOperator {{.*}} <col:31, col:32> 'unsigned int' postfix '++'
  // CHECK: |   `-DeclRefExpr {{.*}} <col:31> 'unsigned int' lvalue bitfield Binding {{.*}} 'o' 'unsigned int'
  // CHECK: |-DeclStmt
  // CHECK:   `-DecompositionDecl {{.*}} used 'insn_type &' cinit
  // CHECK:     |-BindingDecl {{.*}}  col:6 referenced o 'unsigned int'
  // CHECK:     | `-MemberExpr {{.*}} <col:6> 'unsigned int' lvalue bitfield .opc
  // CHECK:     `-BindingDecl {{.*}} <col:9> col:9 referenced i 'unsigned int'
  // CHECK:       `-MemberExpr {{.*}} <col:9> 'unsigned int' lvalue bitfield .imm
  // CHECK: `-BinaryOperator {{.*}} <col:16, col:22> 'bool' '<'
  // CHECK:   |-BinaryOperator {{.*}} <col:16, col:18> 'int' '+'
  // CHECK:   | |-ImplicitCastExpr {{.*}} <col:16> 'int' <IntegralCast>
  // CHECK:   | | `-ImplicitCastExpr {{.*}} <col:16> 'unsigned int' <LValueToRValue>
  // CHECK:   | |   `-DeclRefExpr {{.*}} <col:16> 'unsigned int' lvalue bitfield Binding {{.*}} 'o' 'unsigned int'
  // CHECK:   | `-ImplicitCastExpr {{.*}} <col:18> 'int' <IntegralCast>
  // CHECK:   |   `-ImplicitCastExpr {{.*}} <col:18> 'unsigned int' <LValueToRValue>
  // CHECK:   |     `-DeclRefExpr {{.*}} <col:18> 'unsigned int' lvalue bitfield Binding {{.*}} 'i' 'unsigned int'
  // CHECK:   `-IntegerLiteral {{.*}} <col:22> 'int' 12
}

using size_t = decltype(sizeof(0));

namespace std { template<typename T> struct tuple_size; }
namespace std { template<size_t, typename> struct tuple_element; }

struct C { template<int> int get() const; };
template<> struct std::tuple_size<C> { static const int value = 2; };
template<> struct std::tuple_element<0, C> { typedef int type; };
template<> struct std::tuple_element<1, C> { typedef float type; };

void stbind_tuple() {
  inspect(C()) {
    [3, 2.3] =>;
  };
  // CHECK: `-BinaryOperator {{.*}} 'bool' '&&'
  // CHECK:   |-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:   | |-ImplicitCastExpr {{.*}} 'std::tuple_element<0, C>::type':'int' <LValueToRValue>
  // CHECK:   | | `-DeclRefExpr {{.*}} <col:11> 'std::tuple_element<0, C>::type':'int' lvalue Var {{.*}} '__pat_0_0' 'std::tuple_element<0, C>::type &&'
  // CHECK:   `-BinaryOperator {{.*}} 'bool' '=='
  // CHECK:     | `-ImplicitCastExpr {{.*}} <col:11> 'std::tuple_element<1, C>::type':'float' <LValueToRValue>
  // CHECK:     |   `-DeclRefExpr {{.*}} <col:11> 'std::tuple_element<1, C>::type':'float' lvalue Var {{.*}} '__pat_0_1' 'std::tuple_element<1, C>::type &&'
}