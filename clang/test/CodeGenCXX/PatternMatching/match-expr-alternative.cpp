// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fpattern-matching -O0 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

struct Base { virtual ~Base() = default; };

struct DerivedA : Base {
  int x;
  DerivedA(int x) : x(x) {}
};

struct DerivedB : Base {
  char c;
  DerivedB(char c) : c(c) {}
};

auto alternative_pattern_const(const Base &base) {
  return base match {
    DerivedA: let a => a.x * 2;
  };
}

// CHECK-LABEL: _Z25alternative_pattern_constRK4Base
// CHECK:         %[[VAL_0:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_1:.*]] = alloca i32, align 4
// CHECK:         %[[VAL_2:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_3:.*]] = alloca ptr, align 8
// CHECK:         %[[VAL_4:.*]] = alloca i1, align 8
// CHECK:         %[[VAL_5:.*]] = alloca ptr, align 8
// CHECK:         store ptr %[[VAL_6:.*]], ptr %[[VAL_0]], align 8
// CHECK:         %[[VAL_7:.*]] = load ptr, ptr %[[VAL_0]], align 8
// CHECK:         store ptr %[[VAL_7]], ptr %[[VAL_2]], align 8
// CHECK:         %[[VAL_8:.*]] = load ptr, ptr %[[VAL_0]], align 8
// CHECK:         %[[VAL_9:.*]] = icmp eq ptr %[[VAL_8]], null
// CHECK:         br i1 %[[VAL_9]], label %[[VAL_10:.*]], label %[[VAL_11:.*]]
// CHECK:       dynamic_cast.notnull:
// CHECK:         %[[VAL_13:.*]] = call ptr @__dynamic_cast(ptr %[[VAL_8]], ptr @_ZTI4Base, ptr @_ZTI8DerivedA, i64 0) #2
// CHECK:         br label %[[VAL_14:.*]]
// CHECK:       dynamic_cast.null:
// CHECK:         br label %[[VAL_14]]
// CHECK:       dynamic_cast.end:
// CHECK:         %[[VAL_15:.*]] = phi ptr [ %[[VAL_13]], %[[VAL_11]] ], [ null, %[[VAL_10]] ]
// CHECK:         store ptr %[[VAL_15]], ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_16:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         %[[VAL_17:.*]] = icmp ne ptr %[[VAL_16]], null
// CHECK:         br i1 %[[VAL_17]], label %[[VAL_18:.*]], label %[[VAL_19:.*]]
// CHECK:       match.alt.type.check.pass:
// CHECK:         %[[VAL_20:.*]] = load ptr, ptr %[[VAL_3]], align 8
// CHECK:         store ptr %[[VAL_20]], ptr %[[VAL_5]], align 8
// CHECK:         store i1 true, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_21:.*]]
// CHECK:       match.alt.type.check.fail:
// CHECK:         store i1 false, ptr %[[VAL_4]], align 8
// CHECK:         br label %[[VAL_21]]
// CHECK:       match.alt.end:
// CHECK:         %[[VAL_22:.*]] = load i1, ptr %[[VAL_4]], align 8
// CHECK:         br i1 %[[VAL_22]], label %[[VAL_23:.*]], label %[[VAL_24:.*]]
// CHECK:       match.select.action:
// CHECK:         %[[VAL_25:.*]] = load ptr, ptr %[[VAL_5]], align 8
// CHECK:         %[[VAL_26:.*]] = getelementptr inbounds nuw %[[VAL_27:.*]], ptr %[[VAL_25]], i32 0, i32 1
// CHECK:         %[[VAL_28:.*]] = load i32, ptr %[[VAL_26]], align 8
// CHECK:         %[[VAL_29:.*]] = mul nsw i32 %[[VAL_28]], 2
// CHECK:         store i32 %[[VAL_29]], ptr %[[VAL_1]], align 4
// CHECK:         br label %[[VAL_24]]
// CHECK:       match.select.end:
// CHECK:         %[[VAL_30:.*]] = load i32, ptr %[[VAL_1]], align 4
// CHECK:         ret i32 %[[VAL_30]]

struct Variant {
  Variant(int x) : i(0), x(x) {}
  Variant(double y) : i(1), y(y) {}
  Variant(float z) : i(2), z(z) {}

  constexpr int index() const { return i; }

  template <int I>
  constexpr const auto& get() const {
    if constexpr (I == 0) return x;
    else if constexpr (I == 1) return y;
    else if constexpr (I == 2) return z;
    else static_assert(false);
  }

  int i;

  int x;
  double y;
  float z;
};

namespace std {
  template <typename T>
  struct variant_size;

  template <typename T>
  struct variant_size<const T> {
    static constexpr int value = std::variant_size<T>::value;
  };

  template <>
  struct variant_size<Variant> {
    static constexpr int value = 3;
  };

  template <int I, typename T>
  struct variant_alternative;

  template <int I, class T>
  struct variant_alternative<I, const T> {
    using type = typename std::variant_alternative<I, T>::type const;
  };

  template <> struct variant_alternative<0, Variant> { using type = int; };
  template <> struct variant_alternative<1, Variant> { using type = double; };
  template <> struct variant_alternative<2, Variant> { using type = float; };
}

int variant_like_alternative_pattern(const Variant &var) {
  return var match {
    int: 0 => 0;
  };
}

// Make sure to emit std::variant_alternative<...>& __temp = get<index-of-int>(var)
// right after type check passes.

// CHECK-LABEL: _Z32variant_like_alternative_patternRK7Variant
// CHECK: match.alt.type.check.pass:
// CHECK-NEXT: load ptr, ptr %{{.*}}, align 8
// CHECK-NEXT: call {{.*}} ptr @_ZNK7Variant3getILi0EEERKDav