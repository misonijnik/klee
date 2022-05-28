/*
 * This source file has been created by Yummy Research Team. Copyright (c) 2022
 */

//===-- PForest.h -------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_PFOREST_H
#define KLEE_PFOREST_H

#include "PTree.h"

namespace klee {
  class ExecutionState;
  class PTreeNode;
  class PTree;

  class PForest {
    // Number of registered ID
    int registeredIds = 0;

  public:
    std::map<uint32_t, std::unique_ptr<PTree>> trees;
    PForest() = default;
    ~PForest() = default;
    void addRoot(ExecutionState *initialState);
    void attach(PTreeNode *node, ExecutionState *leftState,
                ExecutionState *rightState);
    void remove(PTreeNode *node);
    void dump(llvm::raw_ostream &os);
    std::uint8_t getNextId() {
      std::uint8_t id = 1 << registeredIds++;
      if (registeredIds > PtrBitCount) {
        klee_error("PForest cannot support more than %d RandomPathSearchers",
                   PtrBitCount);
      }
      return id;
    }
  };
}

#endif /* KLEE_PFOREST_H */
