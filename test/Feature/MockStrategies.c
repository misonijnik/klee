// REQUIRES: z3
// RUN: %clang %s -g -emit-llvm %O0opt -c -o %t.bc

// RUN: rm -rf %t.klee-out-1
// RUN: %klee --output-dir=%t.klee-out-1 --external-calls=all %t.bc 2>&1 | FileCheck %s -check-prefix=CHECK-1
// CHECK-1: failed external call
// CHECK-1: KLEE: done: completed paths = 1
// CHECK-1: KLEE: done: generated tests = 2

// RUN: rm -rf %t.klee-out-2
// RUN: %klee --output-dir=%t.klee-out-2 --mock-policy=all %t.bc 2>&1 | FileCheck %s -check-prefix=CHECK-2
// CHECK-2: ASSERTION FAIL
// CHECK-2: KLEE: done: completed paths = 2
// CHECK-2: KLEE: done: generated tests = 3

// RUN: rm -rf %t.klee-out-3
// RUN: not %klee --output-dir=%t.klee-out-3 --solver-backend=stp --mock-policy=all --mock-strategy=deterministic %t.bc 2>&1 | FileCheck %s -check-prefix=CHECK-3
// CHECK-3: KLEE: ERROR: Deterministic mocks can be generated with Z3 solver only.

// RUN: rm -rf %t.klee-out-4
// RUN: %klee --output-dir=%t.klee-out-4 --solver-backend=z3 --mock-policy=all --mock-strategy=deterministic %t.bc 2>&1 | FileCheck %s -check-prefix=CHECK-4
// CHECK-4: KLEE: done: completed paths = 2
// CHECK-4: KLEE: done: generated tests = 2

#include <assert.h>
#include <klee/klee.h>

extern int foo(int x, int y);

int main() {
  int a, b;
  klee_make_symbolic(&a, sizeof(a), "a");
  klee_make_symbolic(&b, sizeof(b), "b");
  if (a == b && foo(a + b, b) != foo(2 * b, a)) {
    assert(0);
  }
  return 0;
}
