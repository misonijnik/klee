//===-- CFGDistance.cpp ---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Module/CFGDistance.h"
#include "llvm/IR/CFG.h"

using namespace klee;

void CFGDistance::calculateDistance(KBlock *bb) {
  auto blockMap = bb->parent->blockMap;
  std::map<KBlock*, unsigned int> &dist = blockDistance[bb];
  std::vector<std::pair<KBlock *, unsigned>> &sort = blockSortedDistance[bb];
  std::deque<KBlock*> nodes;
  nodes.push_back(bb);
  dist[bb] = 0;
  sort.push_back({bb, 0});
  while(!nodes.empty()) {
    KBlock *currBB = nodes.front();
    for (auto const &succ : successors(currBB->basicBlock)) {
      if (dist.find(blockMap[succ]) == dist.end()) {
        dist[blockMap[succ]] = dist[currBB] + 1;
        sort.push_back({blockMap[succ], dist[currBB] + 1});
        nodes.push_back(blockMap[succ]);
      }
    }
    nodes.pop_front();
  }
}

void CFGDistance::calculateBackwardDistance(KBlock *bb) {
  auto blockMap = bb->parent->blockMap;
  std::map<KBlock*, unsigned int> &bdist = blockBackwardDistance[bb];
  std::vector<std::pair<KBlock *, unsigned>> &bsort =
      blockSortedBackwardDistance[bb];
  std::deque<KBlock*> nodes;
  nodes.push_back(bb);
  bdist[bb] = 0;
  bsort.push_back({bb, 0});
  while(!nodes.empty()) {
    KBlock *currBB = nodes.front();
    for (auto const &pred : predecessors(currBB->basicBlock)) {
      if (bdist.find(blockMap[pred]) == bdist.end()) {
        bdist[blockMap[pred]] = bdist[currBB] + 1;
        bsort.push_back({blockMap[pred], bdist[currBB] + 1});
        nodes.push_back(blockMap[pred]);
      }
    }
    nodes.pop_front();
  }
}

void CFGDistance::calculateDistance(KFunction *kf) {
  auto functionMap = kf->parent->functionMap;
  std::map<KFunction*, unsigned int> &dist = functionDistance[kf];
  std::vector<std::pair<KFunction *, unsigned>> &sort =
      functionSortedDistance[kf];
  std::deque<KFunction*> nodes;
  nodes.push_back(kf);
  dist[kf] = 0;
  sort.push_back({kf, 0});
  while(!nodes.empty()) {
    KFunction *currKF = nodes.front();
    for (auto &callBlock : currKF->kCallBlocks) {
      if (!callBlock->calledFunction ||
          callBlock->calledFunction->isDeclaration()) {
        continue;
      }
      KFunction *callKF = functionMap[callBlock->calledFunction];
      if (dist.find(callKF) == dist.end()) {
        dist[callKF] = dist[callKF] + 1;
        sort.push_back({callKF, dist[currKF] + 1});
        nodes.push_back(callKF);
      }
    }
    nodes.pop_front();
  }
}

void CFGDistance::calculateBackwardDistance(KFunction *kf) {
  auto functionMap = kf->parent->functionMap;
  auto callMap = kf->parent->callMap;
  std::map<KFunction*, unsigned int> &bdist = functionBackwardDistance[kf];
  std::vector<std::pair<KFunction *, unsigned>> &bsort =
      functionSortedBackwardDistance[kf];
  std::deque<KFunction*> nodes;
  nodes.push_back(kf);
  bdist[kf] = 0;
  bsort.push_back({kf, 0});
  while(!nodes.empty()) {
    KFunction *currKF = nodes.front();
    for (auto &cf : callMap[currKF->function]) {
      if (cf->isDeclaration()) {
        continue;
      }
      KFunction *callKF = functionMap[cf];
      if (bdist.find(callKF) == bdist.end()) {
        bdist[callKF] = bdist[callKF] + 1;
        bsort.push_back({callKF, bdist[currKF] + 1});
        nodes.push_back(callKF);
      }
    }
    nodes.pop_front();
  }
}

const std::map<KBlock *, unsigned> &CFGDistance::getDistance(KBlock *kb) {
  if (blockDistance.find(kb) == blockDistance.end())
    calculateDistance(kb);
  return blockDistance[kb];
}

const std::map<KBlock *, unsigned> &
CFGDistance::getBackwardDistance(KBlock *kb) {
  if (blockBackwardDistance.find(kb) == blockBackwardDistance.end())
    calculateBackwardDistance(kb);
  return blockBackwardDistance[kb];
}

const std::vector<std::pair<KBlock *, unsigned int>> &
CFGDistance::getSortedDistance(KBlock *kb) {
  if (blockDistance.find(kb) == blockDistance.end())
    calculateDistance(kb);
  return blockSortedDistance[kb];
}

const std::vector<std::pair<KBlock *, unsigned int>> &
CFGDistance::getSortedBackwardDistance(KBlock *kb) {
  if (blockBackwardDistance.find(kb) == blockBackwardDistance.end())
    calculateBackwardDistance(kb);
  return blockSortedBackwardDistance[kb];
}

const std::map<KFunction *, unsigned> &CFGDistance::getDistance(KFunction *kf) {
  if (functionDistance.find(kf) == functionDistance.end())
    calculateDistance(kf);
  return functionDistance[kf];
}

const std::map<KFunction *, unsigned> &
CFGDistance::getBackwardDistance(KFunction *kf) {
  if (functionBackwardDistance.find(kf) == functionBackwardDistance.end())
    calculateBackwardDistance(kf);
  return functionBackwardDistance[kf];
}

const std::vector<std::pair<KFunction *, unsigned int>> &
CFGDistance::getSortedDistance(KFunction *kf) {
  if (functionDistance.find(kf) == functionDistance.end())
    calculateDistance(kf);
  return functionSortedDistance[kf];
}

const std::vector<std::pair<KFunction *, unsigned int>> &
CFGDistance::getSortedBackwardDistance(KFunction *kf) {
  if (functionBackwardDistance.find(kf) == functionBackwardDistance.end())
    calculateBackwardDistance(kf);
  return functionSortedBackwardDistance[kf];
}
