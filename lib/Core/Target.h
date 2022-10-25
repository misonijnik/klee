//===-- Target.h ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TARGET_H
#define KLEE_TARGET_H

#include "ExecutionState.h"
#include "PTree.h"
#include "klee/ADT/RNG.h"
#include "klee/Module/KModule.h"
#include "klee/System/Time.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <queue>
#include <set>
#include <vector>

namespace klee {
struct Target {
private:
  KBlock *block;

public:
  explicit Target(KBlock *_block) : block(_block) {}

  bool operator<(const Target &other) const { return block < other.block; }

  bool operator==(const Target &other) const { return block == other.block; }

  bool atReturn() const { return isa<KReturnBlock>(block); }

  KBlock *getBlock() const { return block; }

  bool isNull() const { return block == nullptr; }

  explicit operator bool() const noexcept { return !isNull(); }

  std::string print() const;
};
} // namespace klee

#endif /* KLEE_TARGET_H */
