#include "klee/Core/Interpreter.h"

#include "Executor.h"

namespace klee {

Interpreter *Interpreter::create(llvm::LLVMContext &ctx, const InterpreterOptions &opts,
                                 InterpreterHandler *ih) {
  
  Interpreter* res = new Executor(ctx,opts,ih);
  return res;
  
}

} // namespace klee
