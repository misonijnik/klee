//===-- DistanceCalculator.cpp --------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DistanceCalculator.h"
#include "ExecutionState.h"
#include "klee/Module/CodeGraphDistance.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/Target.h"

#include <limits>

using namespace llvm;
using namespace klee;

bool DistanceResult::operator<(const DistanceResult &b) const {
  if (isInsideFunction != b.isInsideFunction)
    return isInsideFunction;
  if (result == WeightResult::Continue && b.result == WeightResult::Continue)
    return weight < b.weight;
  return result < b.result;
}

std::string DistanceResult::toString() const {
  std::ostringstream out;
  out << "(" << (int)!isInsideFunction << ", " << (int)result << ", " << weight
      << ")";
  return out.str();
}

unsigned DistanceCalculator::SpeculativeState::computeHash() {
  unsigned res =
      (reinterpret_cast<uintptr_t>(pc) * SymbolicSource::MAGIC_HASH_CONSTANT) +
      kind;
  res = (res * SymbolicSource::MAGIC_HASH_CONSTANT) + error;
  hashValue = res;
  return hashValue;
}

DistanceResult DistanceCalculator::getDistance(const ExecutionState &state,
                                               ref<Target> target) {
  return getDistance(state.prevPC, state.pc, state.stack.uniqueFrames(),
                     state.error, target);
}

DistanceResult DistanceCalculator::getDistance(const KInstruction *pc,
                                               TargetKind kind,
                                               ReachWithError error,
                                               ref<Target> target) {
  const KInstruction *ki = pc;
  SpeculativeState specState(pc, kind, error);
  if (distanceResultCache.count(target) == 0 ||
      distanceResultCache.at(target).count(specState) == 0) {
    auto result = computeDistance(pc, kind, error, target);
    distanceResultCache[target][specState] = result;
  }
  return distanceResultCache.at(target).at(specState);
}

DistanceResult DistanceCalculator::computeDistance(const KInstruction *pc,
                                                   TargetKind kind,
                                                   ReachWithError error,
                                                   ref<Target> target) const {
  const auto &distanceToTargetFunction =
      codeGraphDistance.getBackwardDistance(target->getBlock()->parent);
  weight_type weight = 0;
  WeightResult res = Miss;
  bool isInsideFunction = true;
  switch (kind) {
  case LocalTarget:
    res = tryGetTargetWeight(pc, weight, target);
    break;

  case PreTarget:
    res = tryGetPreTargetWeight(pc, weight, distanceToTargetFunction, target);
    isInsideFunction = false;
    break;

  case PostTarget:
    res = tryGetPostTargetWeight(pc, weight, target);
    isInsideFunction = false;
    break;

  case NoneTarget:
    break;
  }
  if (Done == res && target->shouldFailOnThisTarget()) {
    if (!target->isThatError(error)) {
      res = Continue;
    }
  }
  return DistanceResult(res, weight, isInsideFunction);
}

DistanceResult
DistanceCalculator::getDistance(const KInstruction *prevPC,
                                const KInstruction *pc,
                                const ExecutionStack::call_stack_ty &frames,
                                ReachWithError error, ref<Target> target) {
  weight_type weight = 0;

  if (!target->shouldFailOnThisTarget() && target->atReturn()) {
    if (prevPC->parent == target->getBlock() &&
        prevPC == target->getBlock()->getLastInstruction()) {
      return DistanceResult(Done);
    } else if (pc->parent == target->getBlock()) {
      return DistanceResult(Continue);
    }
  }

  if (target->shouldFailOnThisTarget() && target->isTheSameAsIn(prevPC) &&
      target->isThatError(error)) {
    return DistanceResult(Done);
  }

  KBlock *kb = pc->parent;
  const auto &distanceToTargetFunction =
      codeGraphDistance.getBackwardDistance(target->getBlock()->parent);
  unsigned int minCallWeight = UINT_MAX, minSfNum = UINT_MAX, sfNum = 0;
  for (auto sfi = frames.rbegin(), sfe = frames.rend(); sfi != sfe; sfi++) {
    unsigned callWeight;
    if (distanceInCallGraph(sfi->kf, kb, callWeight, distanceToTargetFunction,
                            target)) {
      callWeight *= 2;
      callWeight += sfNum;

      if (callWeight < minCallWeight) {
        minCallWeight = callWeight;
        minSfNum = sfNum;
      }
    }

    if (sfi->caller) {
      kb = sfi->caller->parent;
    }
    sfNum++;

    if (minCallWeight < sfNum)
      break;
  }

  TargetKind kind = NoneTarget;
  if (minCallWeight == 0) {
    kind = LocalTarget;
  } else if (minSfNum == 0) {
    kind = PreTarget;
  } else if (minSfNum != UINT_MAX) {
    kind = PostTarget;
  }

  return getDistance(pc, kind, error, target);
}

bool DistanceCalculator::distanceInCallGraph(
    KFunction *kf, KBlock *kb, unsigned int &distance,
    const std::unordered_map<KFunction *, unsigned int>
        &distanceToTargetFunction,
    ref<Target> target) const {
  distance = UINT_MAX;
  const std::unordered_map<KBlock *, unsigned> &dist =
      codeGraphDistance.getDistance(kb);
  KBlock *targetBB = target->getBlock();
  KFunction *targetF = targetBB->parent;

  if (kf == targetF && dist.count(targetBB) != 0) {
    distance = 0;
    return true;
  }

  for (auto &kCallBlock : kf->kCallBlocks) {
    if (dist.count(kCallBlock) != 0) {
      for (auto &calledFunction : kCallBlock->calledFunctions) {
        KFunction *calledKFunction = kf->parent->functionMap[calledFunction];
        if (distanceToTargetFunction.count(calledKFunction) != 0 &&
            distance > distanceToTargetFunction.at(calledKFunction) + 1) {
          distance = distanceToTargetFunction.at(calledKFunction) + 1;
        }
      }
    }
  }
  return distance != UINT_MAX;
}

WeightResult DistanceCalculator::tryGetLocalWeight(
    const KInstruction *pc, weight_type &weight,
    const std::vector<KBlock *> &localTargets, ref<Target> target) const {
  KFunction *currentKF = pc->parent->parent;
  KBlock *currentKB = pc->parent;
  const std::unordered_map<KBlock *, unsigned> &dist =
      codeGraphDistance.getDistance(currentKB);
  weight = UINT_MAX;
  for (auto &end : localTargets) {
    if (dist.count(end) > 0) {
      unsigned int w = dist.at(end);
      weight = std::min(w, weight);
    }
  }

  if (weight == UINT_MAX)
    return Miss;
  if (weight == 0) {
    return Done;
  }

  return Continue;
}

WeightResult DistanceCalculator::tryGetPreTargetWeight(
    const KInstruction *pc, weight_type &weight,
    const std::unordered_map<KFunction *, unsigned int>
        &distanceToTargetFunction,
    ref<Target> target) const {
  KFunction *currentKF = pc->parent->parent;
  std::vector<KBlock *> localTargets;
  for (auto &kCallBlock : currentKF->kCallBlocks) {
    for (auto &calledFunction : kCallBlock->calledFunctions) {
      KFunction *calledKFunction =
          currentKF->parent->functionMap[calledFunction];
      if (distanceToTargetFunction.count(calledKFunction) > 0) {
        localTargets.push_back(kCallBlock);
      }
    }
  }

  if (localTargets.empty())
    return Miss;

  WeightResult res = tryGetLocalWeight(pc, weight, localTargets, target);
  return res == Done ? Continue : res;
}

WeightResult DistanceCalculator::tryGetPostTargetWeight(
    const KInstruction *pc, weight_type &weight, ref<Target> target) const {
  KFunction *currentKF = pc->parent->parent;
  std::vector<KBlock *> &localTargets = currentKF->returnKBlocks;

  if (localTargets.empty())
    return Miss;

  WeightResult res = tryGetLocalWeight(pc, weight, localTargets, target);
  return res == Done ? Continue : res;
}

WeightResult DistanceCalculator::tryGetTargetWeight(const KInstruction *pc,
                                                    weight_type &weight,
                                                    ref<Target> target) const {
  std::vector<KBlock *> localTargets = {target->getBlock()};
  WeightResult res = tryGetLocalWeight(pc, weight, localTargets, target);
  return res;
}
