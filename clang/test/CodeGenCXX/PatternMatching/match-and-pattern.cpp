// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++2d \
// RUN:   -fpattern-matching -fcxx-exceptions -O0 -emit-llvm %s -o - | \
// RUN:   FileCheck %s

bool test_first(int);
bool test_second(int);

struct first_predicate {
  bool operator()(int value) const { return test_first(value); }
};

struct second_predicate {
  bool operator()(int value) const { return test_second(value); }
};

inline constexpr first_predicate first;
inline constexpr second_predicate second;

int short_circuit(int value) {
  return match (value) {
    case first and second => 1;
    case _ => 0;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z13short_circuiti(
// CHECK: call{{.*}} @_ZNK15first_predicateclEi
// CHECK: br i1 {{.*}}, label %[[NEXT:match.and.next.*]], label %[[FAIL:match.and.fail.*]]
// CHECK: [[NEXT]]:
// CHECK: call{{.*}} @_ZNK16second_predicateclEi
// CHECK: [[FAIL]]:

struct Copy {
  int value;
  Copy(const Copy &);
  ~Copy();
};

struct accepts_copy {
  bool operator()(const Copy &) const;
};

inline constexpr accepts_copy accepts;

int declaration_before_rhs(Copy &value) {
  return match (value) {
    case Copy copy and accepts => copy.value;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z22declaration_before_rhsR4Copy(
// CHECK: call void @_ZN4CopyC1ERKS_(
// CHECK: call{{.*}} @_ZNK12accepts_copyclERK4Copy(
// CHECK: call void @_ZN4CopyD1Ev(

struct TaggedCopy {
  int tag;
  Copy copy;
};

int conditional_declaration(TaggedCopy &value) {
  return match (value) {
    case [0, Copy copy] and _ => copy.value;
    case _ => -1;
  };
}

// CHECK-LABEL: define{{.*}} i32 @_Z23conditional_declarationR10TaggedCopy(
// CHECK: store i1 false, ptr %[[ACTIVE:match.decl.active]], align 1
// CHECK: call void @_ZN4CopyC1ERKS_(
// CHECK: store i1 true, ptr %[[ACTIVE]], align 1
// CHECK: load i1, ptr %[[ACTIVE]], align 1
// CHECK: br i1 {{.*}}, label %[[CLEANUP:match.decl.cleanup.*]], label %[[DONE:match.decl.cleanup.done.*]]
// CHECK: [[CLEANUP]]:
// CHECK: call void @_ZN4CopyD1Ev(
// CHECK: [[DONE]]:
