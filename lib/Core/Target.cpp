//===-- Target.cpp --------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Target.h"
#include "ExecutionState.h"

#include "klee/Module/CodeGraphDistance.h"
#include "klee/Module/KInstruction.h"

#include <set>
#include <vector>

using namespace llvm;
using namespace klee;

std::string Target::toString() const {
  std::string repr = "Target: ";
  repr += block->getAssemblyLocation();
  if (atReturn()) {
    repr += " (at the end)";
  }
  return repr;
}

void TargetCalculator::update(const ExecutionState &state) {
  blocksHistory[state.getInitPCBlock()][state.getPrevPCBlock()].insert(
      state.level.begin(), state.level.end());
  transitionsHistory[state.getInitPCBlock()][state.getPrevPCBlock()].insert(
      state.transitionLevel.begin(), state.transitionLevel.end());
}

bool TargetCalculator::differenceIsEmpty(
    const ExecutionState &state,
    const std::map<llvm::BasicBlock *, VisitedBlocks> &history,
    KBlock *target) {
  std::vector<BasicBlock *> diff;
  std::set<BasicBlock *> left(state.level.begin(), state.level.end());
  std::set<BasicBlock *> right(history.at(target->basicBlock).begin(),
                               history.at(target->basicBlock).end());
  std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                      std::inserter(diff, diff.begin()));
  return diff.empty();
}

bool TargetCalculator::differenceIsEmpty(
    const ExecutionState &state,
    const std::map<llvm::BasicBlock *, VisitedTransitions> &history,
    KBlock *target) {
  std::vector<Transition> diff;
  std::set<Transition> left(state.transitionLevel.begin(),
                            state.transitionLevel.end());
  std::set<Transition> right(history.at(target->basicBlock).begin(),
                             history.at(target->basicBlock).end());
  std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                      std::inserter(diff, diff.begin()));
  return diff.empty();
}

Target TargetCalculator::calculateBy(HistoryKind kind, ExecutionState &state) {
  BasicBlock *initialBlock = state.getInitPCBlock();
  std::map<llvm::BasicBlock *, VisitedBlocks> &history =
      blocksHistory[initialBlock];
  std::map<llvm::BasicBlock *, VisitedTransitions> &transitionHistory =
      transitionsHistory[initialBlock];
  BasicBlock *bb = state.getPCBlock();
  KFunction *kf = module.functionMap.at(bb->getParent());
  KBlock *kb = kf->blockMap[bb];
  KBlock *nearestBlock = nullptr;
  unsigned int minDistance = UINT_MAX;
  unsigned int sfNum = 0;
  bool newCov = false;
  for (auto sfi = state.stack.rbegin(), sfe = state.stack.rend(); sfi != sfe;
       sfi++, sfNum++) {
    kf = sfi->kf;

    for (const auto &kbd : codeGraphDistance.getSortedDistance(kb)) {
      KBlock *target = kbd.first;
      unsigned distance = kbd.second;
      if ((sfNum > 0 || distance > 0)) {
        if (distance >= minDistance)
          break;
        if (history[target->basicBlock].size() != 0) {
          bool diffIsEmpty = true;
          if (!newCov) {
            switch (kind) {
            case HistoryKind::Blocks:
              diffIsEmpty = differenceIsEmpty(state, history, target);
              break;
            case HistoryKind::Transitions:
              diffIsEmpty = differenceIsEmpty(state, transitionHistory, target);
              break;
            default:
              assert(0 && "unreachable");
              break;
            }
          }

          if (diffIsEmpty) {
            continue;
          }
        } else {
          newCov = true;
        }
        nearestBlock = target;
        minDistance = distance;
      }
    }

    if (nearestBlock) {
      return Target(nearestBlock);
    }

    if (sfi->caller) {
      kb = sfi->caller->parent;
    }
  }

  return Target(nearestBlock);
}

Target TargetCalculator::calculateByBlockHistory(ExecutionState &state) {
  return calculateBy(HistoryKind::Blocks, state);
}

Target TargetCalculator::calculateByTransitionHistory(ExecutionState &state) {
  return calculateBy(HistoryKind::Transitions, state);
}
