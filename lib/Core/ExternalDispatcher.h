//===-- ExternalDispatcher.h ------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXTERNALDISPATCHER_H
#define KLEE_EXTERNALDISPATCHER_H

#include <cstdint>
#include <string>

namespace llvm {
class Instruction;
class LLVMContext;
} // namespace llvm

namespace klee {
class ExternalDispatcherImpl;
struct KCallable;
class ExternalDispatcher {
private:
  ExternalDispatcherImpl *impl;

public:
  ExternalDispatcher(llvm::LLVMContext &ctx);
  ~ExternalDispatcher();

  /* Call the given function using the parameter passing convention of
   * ci with arguments in args[1], args[2], ... and writing the result
   * into args[0].
   */
  bool executeCall(KCallable *callable, llvm::Instruction *i, uint64_t *args,
                   int roundingMode);
  void *resolveSymbol(const std::string &name);

  int getLastErrno();
  void setLastErrno(int newErrno);
};
} // namespace klee

#endif /* KLEE_EXTERNALDISPATCHER_H */
