//===-- CodeGraphDistance.cpp
//---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Module/CodeGraphDistance.h"
#include "llvm/IR/CFG.h"

#include <deque>
#include <queue>
#include <unordered_map>

using namespace klee;
using namespace llvm;

void CodeGraphDistance::calculateDistance(KBlock *bb) {
  auto blockMap = bb->parent->blockMap;
  std::unordered_map<KBlock *, unsigned int> &dist = blockDistance[bb];
  std::vector<std::pair<KBlock *, unsigned>> &sort = blockSortedDistance[bb];
  std::deque<KBlock *> nodes;
  nodes.push_back(bb);
  dist[bb] = 0;
  sort.push_back({bb, 0});
  while (!nodes.empty()) {
    KBlock *currBB = nodes.front();
    for (auto const &succ : successors(currBB->basicBlock)) {
      if (dist.count(blockMap[succ]) == 0) {
        dist[blockMap[succ]] = dist[currBB] + 1;
        sort.push_back({blockMap[succ], dist[currBB] + 1});
        nodes.push_back(blockMap[succ]);
      }
    }
    nodes.pop_front();
  }
}

void CodeGraphDistance::calculateBackwardDistance(KBlock *bb) {
  auto blockMap = bb->parent->blockMap;
  std::unordered_map<KBlock *, unsigned int> &bdist = blockBackwardDistance[bb];
  std::vector<std::pair<KBlock *, unsigned>> &bsort =
      blockSortedBackwardDistance[bb];
  std::deque<KBlock *> nodes;
  nodes.push_back(bb);
  bdist[bb] = 0;
  bsort.push_back({bb, 0});
  while (!nodes.empty()) {
    KBlock *currBB = nodes.front();
    for (auto const &pred : predecessors(currBB->basicBlock)) {
      if (bdist.count(blockMap[pred]) == 0) {
        bdist[blockMap[pred]] = bdist[currBB] + 1;
        bsort.push_back({blockMap[pred], bdist[currBB] + 1});
        nodes.push_back(blockMap[pred]);
      }
    }
    nodes.pop_front();
  }
}

void CodeGraphDistance::calculateDistance(KFunction *kf) {
  auto &functionMap = kf->parent->functionMap;
  std::unordered_map<KFunction *, unsigned int> &dist = functionDistance[kf];
  std::vector<std::pair<KFunction *, unsigned>> &sort =
      functionSortedDistance[kf];
  std::deque<KFunction *> nodes;
  nodes.push_back(kf);
  dist[kf] = 0;
  sort.push_back({kf, 0});
  while (!nodes.empty()) {
    KFunction *currKF = nodes.front();
    for (auto &callBlock : currKF->kCallBlocks) {
      for (auto &calledFunction : callBlock->calledFunctions) {
        if (!calledFunction || calledFunction->isDeclaration()) {
          continue;
        }
        KFunction *callKF = functionMap[calledFunction];
        if (dist.count(callKF) == 0) {
          dist[callKF] = dist[currKF] + 1;
          sort.push_back({callKF, dist[currKF] + 1});
          nodes.push_back(callKF);
        }
      }
    }
    nodes.pop_front();
  }
}

void CodeGraphDistance::calculateBackwardDistance(KFunction *kf) {
  auto &functionMap = kf->parent->functionMap;
  auto &callMap = kf->parent->callMap;
  std::unordered_map<KFunction *, unsigned int> &bdist =
      functionBackwardDistance[kf];
  std::vector<std::pair<KFunction *, unsigned>> &bsort =
      functionSortedBackwardDistance[kf];
  std::deque<KFunction *> nodes;
  nodes.push_back(kf);
  bdist[kf] = 0;
  bsort.push_back({kf, 0});
  while (!nodes.empty()) {
    KFunction *currKF = nodes.front();
    for (auto &cf : callMap[currKF->function]) {
      if (cf->isDeclaration()) {
        continue;
      }
      KFunction *callKF = functionMap[cf];
      if (bdist.count(callKF) == 0) {
        bdist[callKF] = bdist[currKF] + 1;
        bsort.push_back({callKF, bdist[currKF] + 1});
        nodes.push_back(callKF);
      }
    }
    nodes.pop_front();
  }
}

const std::unordered_map<KBlock *, unsigned> &
CodeGraphDistance::getDistance(KBlock *kb) {
  if (blockDistance.count(kb) == 0)
    calculateDistance(kb);
  return blockDistance.at(kb);
}

const std::unordered_map<KBlock *, unsigned> &
CodeGraphDistance::getBackwardDistance(KBlock *kb) {
  if (blockBackwardDistance.count(kb) == 0)
    calculateBackwardDistance(kb);
  return blockBackwardDistance.at(kb);
}

const std::vector<std::pair<KBlock *, unsigned int>> &
CodeGraphDistance::getSortedDistance(KBlock *kb) {
  if (blockDistance.count(kb) == 0)
    calculateDistance(kb);
  return blockSortedDistance.at(kb);
}

const std::vector<std::pair<KBlock *, unsigned int>> &
CodeGraphDistance::getSortedBackwardDistance(KBlock *kb) {
  if (blockBackwardDistance.count(kb) == 0)
    calculateBackwardDistance(kb);
  return blockSortedBackwardDistance.at(kb);
}

const std::unordered_map<KFunction *, unsigned> &
CodeGraphDistance::getDistance(KFunction *kf) {
  if (functionDistance.count(kf) == 0)
    calculateDistance(kf);
  return functionDistance.at(kf);
}

const std::unordered_map<KFunction *, unsigned> &
CodeGraphDistance::getBackwardDistance(KFunction *kf) {
  if (functionBackwardDistance.count(kf) == 0)
    calculateBackwardDistance(kf);
  return functionBackwardDistance.at(kf);
}

const std::vector<std::pair<KFunction *, unsigned int>> &
CodeGraphDistance::getSortedDistance(KFunction *kf) {
  if (functionDistance.count(kf) == 0)
    calculateDistance(kf);
  return functionSortedDistance.at(kf);
}

const std::vector<std::pair<KFunction *, unsigned int>> &
CodeGraphDistance::getSortedBackwardDistance(KFunction *kf) {
  if (functionBackwardDistance.count(kf) == 0)
    calculateBackwardDistance(kf);
  return functionSortedBackwardDistance.at(kf);
}

KBlock *CodeGraphDistance::getNearestJoinBlock(KBlock *kb) {
  for (auto &kbd : getSortedBackwardDistance(kb)) {
#if LLVM_VERSION_CODE >= LLVM_VERSION(9, 0)
    if (kbd.first->basicBlock->hasNPredecessorsOrMore(2) ||
        kbd.first->basicBlock->hasNPredecessors(0))
      return kbd.first;
#else
    if (kbd.first->basicBlock->hasNUsesOrMore(2) ||
        kbd.first->basicBlock->hasNUses(0))
      return kbd.first;
#endif
  }
  return nullptr;
}

KBlock *CodeGraphDistance::getNearestJoinOrCallBlock(KBlock *kb) {
  for (auto &kbd : getSortedBackwardDistance(kb)) {
#if LLVM_VERSION_CODE >= LLVM_VERSION(9, 0)
    if (kbd.first->basicBlock->hasNPredecessorsOrMore(2) ||
        kbd.first->basicBlock->hasNPredecessors(0) ||
        (isa<KCallBlock>(kbd.first) &&
         dyn_cast<KCallBlock>(kbd.first)->internal() &&
         !dyn_cast<KCallBlock>(kbd.first)->intrinsic()))
      return kbd.first;
#else
    if (kbd.first->basicBlock->hasNUsesOrMore(2) ||
        kbd.first->basicBlock->hasNUses(0) ||
        (isa<KCallBlock>(kbd.first) &&
         dyn_cast<KCallBlock>(kbd.first)->internal() &&
         !dyn_cast<KCallBlock>(kbd.first)->intrinsic()))
      return kbd.first;
#endif
  }
  return nullptr;
}

std::vector<std::pair<KBlock *, KBlock *>>
CodeGraphDistance::dismantle(KBlock *from, std::vector<KBlock *> to) {
  for (auto block : to) {
    assert(from->parent == block->parent &&
           "to and from KBlocks are from different functions.");
  }
  auto kf = from->parent;

  auto distance = getDistance(from);
  std::vector<std::pair<KBlock *, KBlock *>> dismantled;
  std::queue<KBlock *> queue;
  std::unordered_set<KBlock *> used;
  for (auto block : to) {
    used.insert(block);
    queue.push(block);
  }
  while (!queue.empty()) {
    auto block = queue.front();
    queue.pop();
    for (auto const &pred : predecessors(block->basicBlock)) {
      auto nearest = getNearestJoinOrCallBlock(kf->blockMap[pred]);
      if (distance.count(nearest)) {
        if (!used.count(nearest)) {
          used.insert(nearest);
          queue.push(nearest);
        }
        if (std::find(dismantled.begin(), dismantled.end(),
                      std::make_pair(nearest, block)) == dismantled.end()) {
          dismantled.push_back(std::make_pair(nearest, block));
        }
      }
    }
  }
  return dismantled;
}
