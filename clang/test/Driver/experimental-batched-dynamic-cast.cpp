// RUN: %clang -### -c %s 2>&1 | FileCheck %s --check-prefix=OFF
// RUN: %clang -### -c -fexperimental-batched-dynamic-cast %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ON
// RUN: %clang -### -c -fexperimental-batched-dynamic-cast \
// RUN:   -fno-experimental-batched-dynamic-cast %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OFF
// ON: "-cc1"
// ON: "-fexperimental-batched-dynamic-cast"
// OFF: "-cc1"
// OFF-NOT: "-fexperimental-batched-dynamic-cast"

int main() {}
