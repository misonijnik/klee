/*
 * This source file has been modified by Yummy Research Team. Copyright (c) 2022
 */

//===-- MemoryManager.h -----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_MEMORYMANAGER_H
#define KLEE_MEMORYMANAGER_H

#include "klee/Expr/Expr.h"

#include <cstddef>
#include <set>
#include <cstdint>

namespace llvm {
class Value;
}

namespace klee {
class MemoryObject;
class ArrayManager;

class MemoryManager {
private:
  typedef std::set<MemoryObject *> objects_ty;
  objects_ty objects;
  ArrayManager *const arrayManager;

  char *deterministicSpace;
  char *nextFreeSlot;
  size_t spaceSize;

public:
  MemoryManager(ArrayManager *arrayManager);
  ~MemoryManager();

  /**
   * Returns memory object which contains a handle to real virtual process
   * memory.
   */
  MemoryObject *allocate(uint64_t size, bool isLocal, bool isGlobal,
                         const llvm::Value *allocSite, size_t alignment,
                         ref<Expr> lazyInitializedSource = ref<Expr>());
  MemoryObject *allocateFixed(uint64_t address, uint64_t size,
                              const llvm::Value *allocSite);
  MemoryObject *allocateTransparent(uint64_t size, bool isLocal, bool isGlobal,
                         const llvm::Value *allocSite, size_t alignment,
                         ref<Expr> lazyInitializedSource = ref<Expr>());
  void deallocate(MemoryObject *mo);
  void markFreed(MemoryObject *mo);
  ArrayManager *getArrayManager() const { return arrayManager; }

  /*
   * Returns the size used by deterministic allocation in bytes
   */
  size_t getUsedDeterministicSize();
};

} // End klee namespace

#endif /* KLEE_MEMORYMANAGER_H */
