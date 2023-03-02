// RUN: %clang %s -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --execution-mode=error-guided --mock-external-calls --posix-runtime --libc=klee --skip-not-lazy-and-symbolic-pointers --max-time=120s --analysis-reproduce=%s.json %t1.bc
// RUN: FileCheck -input-file=%t.klee-out/warnings.txt %s
#include <stdio.h>


unsigned int *g_status;
unsigned int *MyValue(unsigned short index)
{
  if (index > 5) {
    return NULL;                                  // (1)
  }
  return g_status;
}

void TEST_NullReturns004(unsigned short index)
{
  if (index > 5) {
    return;
  }
  unsigned int *value = MyValue(index);           // (2)
  if (*value == 0) { /* expect NULL_RETURNS */    // CHECK: KLEE: WARNING: 100.00% NullPointerException False Positive at trace 1
    return;
  }
}
