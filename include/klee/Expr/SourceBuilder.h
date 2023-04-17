#ifndef KLEE_SOURCEBUILDER_H
#define KLEE_SOURCEBUILDER_H

#include "klee/ADT/Ref.h"
#include "klee/Expr/SymbolicSource.h"
#include "llvm/IR/Function.h"

namespace klee {

class SourceBuilder {
public:
  SourceBuilder() = delete;

  static ref<SymbolicSource>
  constant(const std::string &name,
           const std::vector<ref<ConstantExpr>> &constantValues);
  static ref<SymbolicSource> symbolicSizeConstant(unsigned defaultValue);
  static ref<SymbolicSource> symbolicSizeConstantAddress(unsigned defaultValue,
                                                         unsigned version);
  static ref<SymbolicSource> makeSymbolic(const std::string &name,
                                          unsigned version);
  static ref<SymbolicSource> lazyInitializationAddress(ref<Expr> pointer);
  static ref<SymbolicSource> lazyInitializationSize(ref<Expr> pointer);
  static ref<SymbolicSource> lazyInitializationContent(ref<Expr> pointer);
  static ref<SymbolicSource> argument(const llvm::Argument &_allocSite,
                                      int _index);
  static ref<SymbolicSource> instruction(const llvm::Instruction &_allocSite,
                                         int _index);
  static ref<SymbolicSource>
  mockDeterministic(const std::string &name,
                    const std::vector<ref<Expr>> &_args,
                    unsigned _returnTypeWidth);
};

}; // namespace klee

#endif /* KLEE_EXPRBUILDER_H */
