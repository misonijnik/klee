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

#include "klee/ADT/RNG.h"
#include "klee/Module/KModule.h"
#include "klee/Module/Locations.h"
#include "klee/System/Time.h"

#include "klee/Support/OptionCategories.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

namespace klee {
class CodeGraphDistance;
class ExecutionState;
class Executor;
struct TargetHash;
struct EquivTargetCmp;
struct TargetCmp;

enum TargetCalculateBy { Default, Blocks, Transitions };

struct Target : LocatedEvent {
private:
  typedef std::unordered_set<Target *, TargetHash, EquivTargetCmp>
      EquivTargetHashSet;
  typedef std::unordered_set<Target *, TargetHash, TargetCmp> TargetHashSet;
  static EquivTargetHashSet cachedTargets;
  static TargetHashSet targets;
  KBlock *block;

  explicit Target(LocatedEvent *_le, KBlock *_block)
      : LocatedEvent(_le ? *_le : LocatedEvent()), block(_block) {}

  static ref<Target> getFromCacheOrReturn(Target *target);

public:
  bool isReported = false;
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

  static ref<Target> create(LocatedEvent *_le, KBlock *_block);
  static ref<Target> create(KBlock *_block);

  int compare(const Target &other) const;

  bool equals(const Target &other) const;

  bool operator<(const Target &other) const;

  bool operator==(const Target &other) const;

  bool atReturn() const { return isa<KReturnBlock>(block); }

  KBlock *getBlock() const { return block; }

  bool isNull() const { return block == nullptr; }

  explicit operator bool() const noexcept { return !isNull(); }

  unsigned hash() const { return reinterpret_cast<uintptr_t>(block); }

  std::string toString() const;
  ~Target();
};

typedef std::pair<llvm::BasicBlock *, llvm::BasicBlock *> Transition;

struct TransitionHash {
  std::size_t operator()(const Transition &p) const {
    return reinterpret_cast<size_t>(p.first) * 31 +
           reinterpret_cast<size_t>(p.second);
  }
};

class TargetCalculator {
  typedef std::unordered_set<llvm::BasicBlock *> VisitedBlocks;
  typedef std::unordered_set<Transition, TransitionHash> VisitedTransitions;

  enum HistoryKind { Blocks, Transitions };

  typedef std::unordered_map<
      llvm::BasicBlock *, std::unordered_map<llvm::BasicBlock *, VisitedBlocks>>
      BlocksHistory;
  typedef std::unordered_map<
      llvm::BasicBlock *,
      std::unordered_map<llvm::BasicBlock *, VisitedTransitions>>
      TransitionsHistory;

public:
  TargetCalculator(const KModule &module, CodeGraphDistance &codeGraphDistance)
      : module(module), codeGraphDistance(codeGraphDistance) {}

  void update(const ExecutionState &state);

  ref<Target> calculate(ExecutionState &state);

private:
  const KModule &module;
  CodeGraphDistance &codeGraphDistance;
  BlocksHistory blocksHistory;
  TransitionsHistory transitionsHistory;

  bool differenceIsEmpty(
      const ExecutionState &state,
      const std::unordered_map<llvm::BasicBlock *, VisitedBlocks> &history,
      KBlock *target);
  bool differenceIsEmpty(
      const ExecutionState &state,
      const std::unordered_map<llvm::BasicBlock *, VisitedTransitions> &history,
      KBlock *target);
};
} // namespace klee

#endif /* KLEE_TARGET_H */
