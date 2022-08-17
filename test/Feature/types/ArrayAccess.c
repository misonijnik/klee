// RUN: %clang %s -emit-llvm -g -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --type-system=CXX --use-tbaa --lazy-instantiation=false --use-gep-expr %t.bc | FileCheck %s

#include "klee/klee.h"
#include <assert.h>
#include <stdio.h>

int main() {
  int array_int[10] = {1, 2, 3, 4, 5};
  float array_float[2] = {1.0, 2.0};

  int *ptr;
  klee_make_symbolic(&ptr, sizeof(ptr), "ptr");
  *ptr = 10;

  // CHECK-NOT: ASSERTION FAIL
  assert((void *)ptr != (void *)array_float);

  // CHECK: x
  if ((void *)ptr == (void *)array_int) {
    printf("x\n");
    return 0;
  }

  float(*ptr_array_float)[2];
  klee_make_symbolic(&ptr_array_float, sizeof(ptr_array_float), "ptr_array_float");
  (*ptr_array_float)[1] = 111.1;

  // CHECK-NOT: ASSERTION FAIL
  assert((void *)ptr_array_float != (void *)&array_int);

  // CHECK: y
  if (ptr_array_float == &array_float) {
    printf("y\n");
    return 2;
  }
  return 1;
}
