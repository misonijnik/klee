int main(int x) {
  int *p = 0;
  while (!p) {
    if (x) {
      p = &x;
    } else {
      p = 0;
    }
  }
  return *p;
}

// RUN: %clang %s -emit-llvm %O0opt -c -g -O0 -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --use-guided-search=error --annotations=%annotations --mock-policy=all --skip-not-symbolic-objects --skip-not-lazy-initialized --check-out-of-memory --search=bfs --max-cycles=1 --use-lazy-initialization=only --analysis-reproduce=%s.json %t1.bc
// RUN: FileCheck -input-file=%t.klee-out/warnings.txt %s -check-prefix=CHECK-NONE
// CHECK-NONE: KLEE: WARNING: {{[0-9][0-9]\.[0-9][0-9]}}% NullPointerException False Positive at trace 1
