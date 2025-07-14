#include "IsolatedStatesInitializer.h"
#include "ProofObligation.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Module/Target.h"
#include "klee/Support/DebugFlags.h"

#include "llvm/IR/Instructions.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stack>
#include <utility>

namespace klee {

std::pair<KInstruction *, std::set<ref<Target>>>
DefaultIsolatedStatesInitializer::selectAction() {
  auto KI = queued.front();
  queued.pop_front();
  auto targets = targetMap[KI];
  assert(!targets.empty());
  targetMap.erase(KI);
  for (auto target : targets) {
    instructionMap[target].erase(KI);
  }
  return {KI, targets};
}

bool DefaultIsolatedStatesInitializer::empty() { return queued.empty(); }

void DefaultIsolatedStatesInitializer::update(const pobs_ty &added,
                                              const pobs_ty &removed) {
  for (auto i : added) {
    addPob(i);
  }
  for (auto i : removed) {
    removePob(i);
  }
}

void DefaultIsolatedStatesInitializer::addPob(ProofObligation *pob) {
  auto target = pob->location;
  knownTargets[target]++;
  if (knownTargets[target] > 1) {
    return; // There has been such a target already
  }

  if (pob->location->getBlock()->parent->entryKBlock !=
      pob->location->getBlock()) {
    auto backstep = cgd->getNearestPredicateSatisfying(
        pob->location->getBlock(), PredicateAdapter(predicate), false);

    for (auto from : backstep) {
      auto toBlocks = cgd->getNearestPredicateSatisfying(
          from, PredicateAdapter(predicate), true);
      for (auto to : toBlocks) {
        KInstruction *fromInst =
            (predicate.isInterestingCallBlock(from) ? from->instructions[1]
                                                    : from->instructions[0]);
        addInit(fromInst, ReachBlockTarget::create(to));
      }
      KInstruction *fromInst =
          (predicate.isInterestingCallBlock(from) ? from->instructions[1]
                                                  : from->instructions[0]);
      addInit(fromInst, target);
    }
  } else {
    for (auto i : allowed) {
      for (auto kcallblock : i->kCallBlocks) {
        if (kcallblock->calledFunctions.count(
                pob->location->getBlock()->parent)) {
          addInit(kcallblock->getFirstInstruction(),
                  ReachBlockTarget::create(pob->location->getBlock()));
          addInit(kcallblock->getFirstInstruction(), target);
        }
      }
    }
  }

  std::list<KInstruction *> enqueue;
  for (auto KI : awaiting) {
    if (targetMap[KI].count(target)) {
      enqueue.push_back(KI);
    }
  }

  for (auto KI : enqueue) {
    awaiting.remove(KI);
    queued.push_back(KI);
  }
}

void DefaultIsolatedStatesInitializer::removePob(ProofObligation *pob) {
  auto target = pob->location;
  assert(knownTargets[target] != 0);
  knownTargets[target]--;

  if (knownTargets[target] > 0) {
    return;
  }

  std::list<KInstruction *> dequeue;
  for (auto KI : queued) {
    bool noKnown = true;
    for (auto target : knownTargets) {
      if (target.second != 0 && targetMap[KI].count(target.first)) {
        noKnown = false;
        break;
      }
    }
    if (noKnown) {
      dequeue.push_back(KI);
    }
  }

  for (auto KI : dequeue) {
    awaiting.push_back(KI);
    queued.remove(KI);
  }
}

void DefaultIsolatedStatesInitializer::initializeFunctions(
    std::set<KFunction *, KFunctionCompare> functions) {
  allowed = functions;
}

void DefaultIsolatedStatesInitializer::addErrorInit(ref<Target> errorTarget) {
  auto errorT = dyn_cast<ReproduceErrorTarget>(errorTarget);
  auto location = errorTarget->getBlock();
  // Check direction
  std::set<KBlock *, KBlockCompare> nearest;
  if (predicate(errorTarget->getBlock()) && !errorT->isThatError(Reachable)) {
    nearest.insert(errorTarget->getBlock()); // HOT FIX
  } else {
    nearest = cgd->getNearestPredicateSatisfying(
        location, PredicateAdapter(predicate), false);
  }
  for (auto i : nearest) {
    KInstruction *from =
        (predicate.isInterestingCallBlock(i) ? i->instructions[1]
                                             : i->instructions[0]);
    auto toBlocks = cgd->getNearestPredicateSatisfying(
        i, PredicateAdapter(predicate), true);
    for (auto to : toBlocks) {
      addInit(from, ReachBlockTarget::create(to));
    }
    if (errorT->isThatError(Reachable)) {
      addInit(from, ReachBlockTarget::create(location));
    } else {
      addInit(from, errorTarget);
    }
  }
}

void DefaultIsolatedStatesInitializer::addInit(KInstruction *from,
                                               ref<Target> to) {
  if (initialized[from].count(to)) {
    return;
  }
  initialized[from].insert(to);

  if (debugPrints.isSet(DebugPrint::Init)) {
    llvm::errs() << "[initializer] From " << from->toString() << " to "
                 << to->toString() << " scheduled\n";
  }

  targetMap[from].insert(to);
  instructionMap[to].insert(from);
  bool awaits =
      (std::find(awaiting.begin(), awaiting.end(), from) != awaiting.end());
  bool enqueued =
      (std::find(queued.begin(), queued.end(), from) != queued.end());

  if (!awaits && !enqueued) {
    if (knownTargets.count(to)) {
      queued.push_back(from);
    } else {
      awaiting.push_back(from);
    }
  } else if (awaits) {
    if (knownTargets.count(to)) {
      awaiting.remove(from);
      queued.push_back(from);
    }
  }
}

}; // namespace klee
