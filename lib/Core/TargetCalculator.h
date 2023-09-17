//===-- TargetCalculator.h --------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TARGETCALCULATOR_H
#define KLEE_TARGETCALCULATOR_H

#include "klee/ADT/RNG.h"
#include "klee/Module/KModule.h"
#include "klee/Module/Target.h"
#include "klee/Module/TargetHash.h"
#include "klee/Support/OptionCategories.h"
#include "klee/System/Time.h"

#include "klee/Support/CompilerWarning.h"
DISABLE_WARNING_PUSH
DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
DISABLE_WARNING_POP

#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

namespace klee {
class CodeGraphDistance;
class ExecutionState;
struct TransitionHash;

enum TargetCalculateBy { Default, Blocks, Transitions };

typedef std::pair<llvm::BasicBlock *, llvm::BasicBlock *> Transition;
typedef std::pair<llvm::BasicBlock *, unsigned> Branch;

class TargetCalculator {
  typedef std::unordered_set<KBlock *> VisitedBlocks;
  typedef std::unordered_set<Transition, TransitionHash> VisitedTransitions;
  typedef std::unordered_set<Branch, BranchHash> VisitedBranches;

  enum HistoryKind { Blocks, Transitions };

  typedef std::unordered_map<
      llvm::Function *, std::unordered_map<llvm::BasicBlock *, VisitedBlocks>>
      BlocksHistory;
  typedef std::unordered_map<
      llvm::Function *,
      std::unordered_map<llvm::BasicBlock *, VisitedTransitions>>
      TransitionsHistory;

  typedef std::unordered_map<
      llvm::Function *,
      std::unordered_map<llvm::BasicBlock *, std::set<unsigned>>>
      CoveredBranches;

  typedef std::unordered_map<llvm::Function *, VisitedBlocks> CoveredBlocks;

public:
  TargetCalculator(CodeGraphDistance &codeGraphDistance)
      : codeGraphDistance(codeGraphDistance) {}

  void update(const ExecutionState &state);

  TargetHashSet calculate(ExecutionState &state);

private:
  CodeGraphDistance &codeGraphDistance;
  BlocksHistory blocksHistory;
  TransitionsHistory transitionsHistory;
  CoveredBranches coveredBranches;
  CoveredBlocks coveredBlocks;

  bool differenceIsEmpty(
      const ExecutionState &state,
      const std::unordered_map<llvm::BasicBlock *, VisitedBlocks> &history,
      KBlock *target);
  bool differenceIsEmpty(
      const ExecutionState &state,
      const std::unordered_map<llvm::BasicBlock *, VisitedTransitions> &history,
      KBlock *target);

  bool uncoveredBlockPredicate(ExecutionState *state, KBlock *kblock);
};
} // namespace klee

#endif /* KLEE_TARGETCALCULATOR_H */
