int main(int x) {
  int *p = 0;
  if (x) {
    p = &x;
  } else {
    klee_sleep();
  }
  return *p;
}
// RUN: %clang %s -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --execution-mode=error-guided --mock-external-calls --skip-not-lazy-and-symbolic-pointers --check-out-of-memory --max-stepped-instructions=19 --max-cycles=0 --analysis-reproduce=%s.json %t1.bc
// RUN: FileCheck -input-file=%t.klee-out/warnings.txt %s -check-prefix=CHECK-NONE
// CHECK-NONE: KLEE: WARNING: 50.00% NullPointerException False Positive at trace 1