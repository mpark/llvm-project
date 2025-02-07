// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -Wno-c++20-extensions -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

struct Lifetime {
  Lifetime(bool *_flag, int n) : flag(_flag), n(n) { *flag = true; }
  ~Lifetime() { *flag = false; }
  bool *flag;
  int n;
};

// CHECK-LABEL: _Z6extendi
bool extend(int n) {
  bool flag = false;
  if (Lifetime(&flag, n) match [? let b, 101]) {
    return b;
  } else if (n == 202) {
    return flag;
  }
  // CHECK: if.end
  // CHECK:   store i32 0, ptr %{{.*}}, align 4
  // CHECK:   br label %cleanup
  // CHECK: cleanup:
  // CHECK:   call void @_ZN8LifetimeD1Ev
  return flag;
}

// CHECK-LABEL: _Z13do_not_extendi
bool do_not_extend(int n) {
  bool flag = false;
  if ((Lifetime(&flag, n) match [? let b, 101])) {
    // CHECK: match.decomp.end
    // CHECK: %[[RES:.*]] = load i1, ptr %{{.*}}, align 8
    // CHECK: call void @_ZN8LifetimeD1Ev
    // CHECK: br i1 %[[RES]], label %if.then, label %if.else
    return flag;
  } else if (n == 202) {
    return flag;
  }
  return flag;
}