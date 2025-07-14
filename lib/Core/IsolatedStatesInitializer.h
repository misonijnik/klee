// -*- C++ -*-
#ifndef KLEE_INITIALIZER_H
#define KLEE_INITIALIZER_H

#include "ProofObligation.h"
#include "klee/Module/CodeGraphInfo.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include <list>
#include <queue>
#include <set>

namespace klee {
struct Conflict;

/**
 * Is responsible for prioritizing the creation of new isolated states
 */
class IsolatedStatesInitializer {
public:
  virtual ~IsolatedStatesInitializer() {}
  virtual std::pair<KInstruction *, std::set<ref<Target>>> selectAction() = 0;
  virtual bool empty() = 0;
  virtual void update(const pobs_ty &added, const pobs_ty &removed) = 0;
};

/**
 * The default initializer that maintains a queue of most wanted initializer
 * pairs (kinstruction to target).
 * Bidirectional mode is only usable in error-guided moded.
 */
class DefaultIsolatedStatesInitializer : public IsolatedStatesInitializer {
public:
  std::pair<KInstruction *, std::set<ref<Target>>> selectAction() override;
  bool empty() override;

  bool initsLeftForTarget(ref<Target> t) {
    return instructionMap.count(t) && !instructionMap.at(t).empty();
  }

  void initializeFunctions(std::set<KFunction *, KFunctionCompare> functions);
  void addErrorInit(ref<Target> errorTarget);

  void update(const pobs_ty &added, const pobs_ty &removed) override;

  explicit DefaultIsolatedStatesInitializer(CodeGraphInfo *cgd,
                                            InitializerPredicate &predicate)
      : cgd(cgd), predicate(predicate){};

  ~DefaultIsolatedStatesInitializer() override {}

private:
  CodeGraphInfo *cgd;
  InitializerPredicate &predicate;

  // There are proof obligation in these targets
  std::map<ref<Target>, unsigned> knownTargets;

  // Targets collected for each initial instruction
  std::map<KInstruction *, std::set<ref<Target>>, KInstructionCompare>
      targetMap;

  // Reverse
  std::map<ref<Target>, std::set<KInstruction *, KInstructionCompare>>
      instructionMap;

  // awaiting until the are proof obligations in one of their targets
  std::list<KInstruction *> awaiting;

  // There are currently proof obligations in their targets so they are
  // queued for dispatch
  std::list<KInstruction *> queued;

  // For every (KI, Target) pair in this map, there is a state that starts
  // at KI and has Target as one of its targets.
  std::map<KInstruction *, std::set<ref<Target>>, KInstructionCompare>
      initialized;

  // Already dismantled functions don't need to be dismantled again
  std::unordered_set<KFunction *> dismantledFunctions;

  std::set<KFunction *, KFunctionCompare> allowed;

  void addInit(KInstruction *from, ref<Target> to);
  void addPob(ProofObligation *pob);
  void removePob(ProofObligation *pob);
};

}; // namespace klee

#endif
