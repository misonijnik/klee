//===-- LocationInfo.cpp ------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Module/LocationInfo.h"
#include "klee/Support/CompilerWarning.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FormattedStream.h"
DISABLE_WARNING_POP

namespace klee {

LocationInfo getLocationInfo(const llvm::Function *func) {
  const auto dsub = func->getSubprogram();

  if (dsub != nullptr) {
    auto path = dsub->getFilename();
    return {path.str(), dsub->getLine(), 0}; // TODO why not use column here?
  }

  return {"", 0, 0};
}

LocationInfo getLocationInfo(const llvm::Instruction *inst) {
  // Retrieve debug information associated with instruction
  const auto &dl = inst->getDebugLoc();

  // Check if a valid debug location is assigned to the instruction.
  if (dl.get() != nullptr) {
    auto full_path = dl->getFilename();
    auto line = dl.getLine();
    auto column = dl.getCol();

    // Still, if the line is unknown, take the context of the instruction to
    // narrow it down
    if (line == 0) {
      if (auto LexicalBlock =
              llvm::dyn_cast<llvm::DILexicalBlock>(dl.getScope())) {
        line = LexicalBlock->getLine();
        column = LexicalBlock->getColumn();
      }
    }
    return {full_path.str(), line, column};
  }

  return getLocationInfo(inst->getParent()->getParent());
}

} // namespace klee
