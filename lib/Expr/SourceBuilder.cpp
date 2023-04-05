#include "klee/Expr/SourceBuilder.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/SymbolicSource.h"

using namespace klee;

ref<SymbolicSource>
SourceBuilder::constant(const std::string &name,
                        const std::vector<ref<ConstantExpr>> &constantValues) {
  ref<SymbolicSource> r(new ConstantSource(name, constantValues));
  r->computeHash();
  return r;
}

ref<SymbolicSource> SourceBuilder::symbolicSizeConstant(unsigned defaultValue) {
  ref<SymbolicSource> r(new SymbolicSizeConstantSource(defaultValue));
  r->computeHash();
  return r;
}

ref<SymbolicSource>
SourceBuilder::symbolicSizeConstantAddress(unsigned defaultValue,
                                           unsigned version) {
  ref<SymbolicSource> r(
      new SymbolicSizeConstantAddressSource(defaultValue, version));
  r->computeHash();
  return r;
}

ref<SymbolicSource> SourceBuilder::makeSymbolic(const std::string &name,
                                                unsigned version) {
  ref<SymbolicSource> r(new MakeSymbolicSource(name, version));
  r->computeHash();
  return r;
}

ref<SymbolicSource>
SourceBuilder::lazyInitializationAddress(ref<Expr> pointer) {
  ref<SymbolicSource> r(new LazyInitializationAddressSource(pointer));
  r->computeHash();
  return r;
}

ref<SymbolicSource> SourceBuilder::lazyInitializationSize(ref<Expr> pointer) {
  ref<SymbolicSource> r(new LazyInitializationSizeSource(pointer));
  r->computeHash();
  return r;
}

ref<SymbolicSource>
SourceBuilder::lazyInitializationContent(ref<Expr> pointer) {
  ref<SymbolicSource> r(new LazyInitializationContentSource(pointer));
  r->computeHash();
  return r;
}

ref<SymbolicSource> SourceBuilder::argument(const llvm::Argument &_allocSite,
                                            int _index) {
  return new ArgumentSource(_allocSite, _index);
}

ref<SymbolicSource>
SourceBuilder::instruction(const llvm::Instruction &_allocSite, int _index) {
  return new InstructionSource(_allocSite, _index);
}
