//===-- TargetedManager.cpp -----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TargetManager.h"

#include "TargetCalculator.h"

#include "klee/Module/KInstruction.h"

#include <cassert>

using namespace llvm;
using namespace klee;

namespace klee {} // namespace klee

void TargetManager::updateMiss(ExecutionState &state, ref<Target> target) {
  auto &stateTargetForest = targetForest(state);
  stateTargetForest.remove(target);
  setTargets(state, stateTargetForest.getTargets());
  if (guidance == Interpreter::GuidanceKind::CoverageGuidance) {
    state.setTargeted(false);
  }
}

void TargetManager::updateContinue(ExecutionState &state, ref<Target> target) {}

void TargetManager::updateDone(ExecutionState &state, ref<Target> target) {
  auto &stateTargetForest = targetForest(state);

  stateTargetForest.stepTo(target);
  setTargets(state, stateTargetForest.getTargets());
  setHistory(state, stateTargetForest.getHistory());
  if (guidance == Interpreter::GuidanceKind::CoverageGuidance ||
      target->shouldFailOnThisTarget()) {
    reachedTargets.insert(target);
    for (auto es : states) {
      if (isTargeted(*es)) {
        auto &esTargetForest = targetForest(*es);
        esTargetForest.block(target);
        if (guidance == Interpreter::GuidanceKind::CoverageGuidance) {
          if (targets(*es).size() == 0) {
            es->setTargeted(false);
          }
        }
        setTargets(*es, esTargetForest.getTargets());
      }
    }
  }
  if (guidance == Interpreter::GuidanceKind::CoverageGuidance) {
    state.setTargeted(false);
  }
}

void TargetManager::updateTargets(ExecutionState &state) {
  if (!state.isolated && guidance == Interpreter::GuidanceKind::CoverageGuidance) {
    if (targets(state).empty() && state.isStuck(MaxCyclesBeforeStuck)) {
      state.setTargeted(true);
    }
    if (isTargeted(state) && targets(state).empty()) {
      ref<Target> target(targetCalculator.calculate(state));
      if (target) {
        state.targetForest.add(target);
        setTargets(state, state.targetForest.getTargets());
      }
    }
  }

  if (!isTargeted(state)) {
    return;
  }

  auto stateTargets = targets(state);
  auto &stateTargetForest = targetForest(state);

  for (auto target : stateTargets) {
    if (!stateTargetForest.contains(target)) {
      continue;
    }

    DistanceResult stateDistance = distance(state, target);
    switch (stateDistance.result) {
    case WeightResult::Continue:
      updateContinue(state, target);
      break;
    case WeightResult::Miss:
      updateMiss(state, target);
      break;
    case WeightResult::Done:
      updateDone(state, target);
      break;
    default:
      assert(0 && "unreachable");
    }
  }
}

void TargetManager::update(ExecutionState *current,
                           const std::vector<ExecutionState *> &addedStates,
                           const std::vector<ExecutionState *> &removedStates) {

  states.insert(addedStates.begin(), addedStates.end());

  for (const auto state : removedStates) {
    states.erase(state);
  }

  if (current && (std::find(removedStates.begin(), removedStates.end(),
                            current) == removedStates.end())) {
    localStates.insert(current);
  }
  for (const auto state : addedStates) {
    localStates.insert(state);
  }
  for (const auto state : removedStates) {
    localStates.insert(state);
  }

  for (auto state : localStates) {
    state->stepTargetsAndHistory();
    auto prevKI = state->prevPC;
    auto kf = prevKI->parent->parent;
    auto kmodule = kf->parent;

    if (prevKI->inst->isTerminator() && kmodule->inMainModule(kf->function)) {
      auto target = Target::create(state->prevPC->parent);
      if (guidance == Interpreter::GuidanceKind::CoverageGuidance) {
        if (!target->atReturn() ||
            state->prevPC == target->getBlock()->getLastInstruction()) {
          setReached(target);
        }
      }
    }

    updateTargets(*state);
  }

  localStates.clear();
}

void TargetManager::update(ref<ObjectManager::Event> e) {
  if (auto states = dyn_cast<ObjectManager::States>(e)) {
    update(states->modified, states->added, states->removed);
  }
}
