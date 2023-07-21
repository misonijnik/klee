//===-- TargetHash.h --------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TARGETHASH_H
#define KLEE_TARGETHASH_H

#include "klee/ADT/Ref.h"

#include <map>
#include <unordered_map>
#include <unordered_set>

namespace llvm {
class BasicBlock;
}

namespace klee {
struct Target;

struct TargetHash {
  unsigned operator()(const ref<Target> &t) const;
};

struct TargetCmp {
  bool operator()(const ref<Target> &a, const ref<Target> &b) const;
};

typedef std::pair<llvm::BasicBlock *, llvm::BasicBlock *> Transition;

struct TransitionHash {
  std::size_t operator()(const Transition &p) const;
};

struct TargetLess {
  bool operator()(const ref<Target> &a, const ref<Target> &b) const {
    return a < b;
  }
};

template <class T>
using TargetHashMap = std::unordered_map<ref<Target>, T, TargetHash, TargetCmp>;

using TargetHashSet = std::unordered_set<ref<Target>, TargetHash, TargetCmp>;

} // namespace klee
#endif /* KLEE_TARGETHASH_H */
