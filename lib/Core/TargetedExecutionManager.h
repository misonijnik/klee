//===-- TargetedExecutionManager.h ------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Class to manage everything for targeted execution mode
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TARGETEDEXECUTIONMANAGER_H
#define KLEE_TARGETEDEXECUTIONMANAGER_H

#include "Target.h"
#include "TargetForest.h"
#include "klee/Module/KModule.h"
#include "klee/Core/TargetedExecutionReporter.h"

#include <unordered_map>

namespace klee {

class TargetedHaltsOnTraces {
  using HaltTypeToConfidence = std::unordered_map<HaltExecution::Reason, confidence::ty>;
  using TraceToHaltTypeToConfidence = std::unordered_map<ref<Target>, HaltTypeToConfidence, RefTargetHash, RefTargetCmp>;
  TraceToHaltTypeToConfidence traceToHaltTypeToConfidence;

  static void
  totalConfidenceAndTopContributor(
  const HaltTypeToConfidence &haltTypeToConfidence,
  confidence::ty *confidence,
  HaltExecution::Reason *reason);

public:
  explicit TargetedHaltsOnTraces(ref<TargetForest> &forest);

  void subtractConfidencesFrom(TargetForest &forest, HaltExecution::Reason reason);

  /* Report for targeted static analysis mode */
  void reportFalsePositives(bool canReachSomeTarget);
};

class TargetedExecutionManager {
  struct TargetPreparator;

public:
  using Targets = std::vector<ref<Target> >;
  using Error2Targets = std::unordered_map<ReachWithError, Targets *>;
  using Location2Targets = std::unordered_map<LocatedEvent *, Error2Targets *>;

private:
  /// Map of blocks to corresponding execution targets
  Location2Targets location2targets;
  std::unordered_set<unsigned> broken_traces;

public:
  ~TargetedExecutionManager();

  std::unordered_map<KFunction *, ref<TargetForest>>
  prepareTargets(KModule *origModule, KModule *kmodule, PathForest *paths);

  void reportFalseNegative(ExecutionState &state, ReachWithError error);

  // Return true if report is successful
  bool reportTruePositive(ExecutionState &state, ReachWithError error);
};

} // End klee namespace

#endif /* KLEE_TARGETEDEXECUTIONMANAGER_H */
