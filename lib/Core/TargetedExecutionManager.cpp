//===-- TargetedExecutionManager.cpp --------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TargetedExecutionManager.h"

#include "DistanceCalculator.h"
#include "TargetManager.h"

#include "ExecutionState.h"
#include "klee/Core/TerminationTypes.h"
#include "klee/Module/CodeGraphDistance.h"
#include "klee/Module/KInstruction.h"
#include "klee/Support/ErrorHandling.h"

#include <memory>

using namespace llvm;
using namespace klee;

namespace klee {
cl::OptionCategory
    TerminationCat("State and overall termination options",
                   "These options control termination of the overall KLEE "
                   "execution and of individual states.");

/*** Termination criteria options ***/

cl::opt<std::string>
    MaxTime("max-time",
            cl::desc("Halt execution after the specified duration.  "
                     "Set to 0s to disable (default=0s)"),
            cl::init("0s"), cl::cat(TerminationCat));

cl::list<StateTerminationType> ExitOnErrorType(
    "exit-on-error-type",
    cl::desc(
        "Stop execution after reaching a specified condition (default=false)"),
    cl::values(clEnumValN(StateTerminationType::Abort, "Abort",
                          "The program crashed (reached abort()/klee_abort())"),
               clEnumValN(StateTerminationType::Assert, "Assert",
                          "An assertion was hit"),
               clEnumValN(StateTerminationType::BadVectorAccess,
                          "BadVectorAccess", "Vector accessed out of bounds"),
               clEnumValN(StateTerminationType::Execution, "Execution",
                          "Trying to execute an unexpected instruction"),
               clEnumValN(StateTerminationType::External, "External",
                          "External objects referenced"),
               clEnumValN(StateTerminationType::Free, "Free",
                          "Freeing invalid memory"),
               clEnumValN(StateTerminationType::Model, "Model",
                          "Memory model limit hit"),
               clEnumValN(StateTerminationType::Overflow, "Overflow",
                          "An overflow occurred"),
               clEnumValN(StateTerminationType::Ptr, "Ptr", "Pointer error"),
               clEnumValN(StateTerminationType::UndefinedBehavior,
                          "UndefinedBehavior", "Undefined behavior detected"),
               clEnumValN(StateTerminationType::ReadOnly, "ReadOnly",
                          "Write to read-only memory"),
               clEnumValN(StateTerminationType::ReportError, "ReportError",
                          "klee_report_error called"),
               clEnumValN(StateTerminationType::User, "User",
                          "Wrong klee_* functions invocation")),
    cl::ZeroOrMore, cl::cat(TerminationCat));

cl::opt<unsigned long long>
    MaxInstructions("max-instructions",
                    cl::desc("Stop execution after this many instructions.  "
                             "Set to 0 to disable (default=0)"),
                    cl::init(0), cl::cat(TerminationCat));

cl::opt<unsigned long long> MaxSteppedInstructions(
    "max-stepped-instructions",
    cl::desc("Stop state execution after this many instructions.  Set to 0 to "
             "disable (default=0)"),
    cl::init(0), cl::cat(TerminationCat));

cl::opt<unsigned> MaxForks(
    "max-forks",
    cl::desc("Only fork this many times.  Set to -1 to disable (default=-1)"),
    cl::init(~0u), cl::cat(TerminationCat));

cl::opt<unsigned> MaxDepth("max-depth",
                           cl::desc("Only allow this many symbolic branches.  "
                                    "Set to 0 to disable (default=0)"),
                           cl::init(0), cl::cat(TerminationCat));

cl::opt<unsigned>
    MaxMemory("max-memory",
              cl::desc("Refuse to fork when above this amount of "
                       "memory (in MB) (see -max-memory-inhibit) and terminate "
                       "states when additional 100MB allocated (default=2000)"),
              cl::init(2000), cl::cat(TerminationCat));

cl::opt<bool> MaxMemoryInhibit("max-memory-inhibit",
                               cl::desc("Inhibit forking when above memory cap "
                                        "(see -max-memory) (default=true)"),
                               cl::init(true), cl::cat(TerminationCat));

cl::opt<unsigned> RuntimeMaxStackFrames(
    "max-stack-frames",
    cl::desc("Terminate a state after this many stack frames.  Set to 0 to "
             "disable (default=8192)"),
    cl::init(8192), cl::cat(TerminationCat));

cl::opt<double> MaxStaticForkPct(
    "max-static-fork-pct", cl::init(1.),
    cl::desc("Maximum percentage spent by an instruction forking out of the "
             "forking of all instructions (default=1.0 (always))"),
    cl::cat(TerminationCat));

cl::opt<double> MaxStaticSolvePct(
    "max-static-solve-pct", cl::init(1.),
    cl::desc("Maximum percentage of solving time that can be spent by a single "
             "instruction over total solving time for all instructions "
             "(default=1.0 (always))"),
    cl::cat(TerminationCat));

cl::opt<double> MaxStaticCPForkPct(
    "max-static-cpfork-pct", cl::init(1.),
    cl::desc("Maximum percentage spent by an instruction of a call path "
             "forking out of the forking of all instructions in the call path "
             "(default=1.0 (always))"),
    cl::cat(TerminationCat));

cl::opt<double> MaxStaticCPSolvePct(
    "max-static-cpsolve-pct", cl::init(1.),
    cl::desc("Maximum percentage of solving time that can be spent by a single "
             "instruction of a call path over total solving time for all "
             "instructions (default=1.0 (always))"),
    cl::cat(TerminationCat));

cl::opt<unsigned> MaxStaticPctCheckDelay(
    "max-static-pct-check-delay",
    cl::desc("Number of forks after which the --max-static-*-pct checks are "
             "enforced (default=1000)"),
    cl::init(1000), cl::cat(TerminationCat));

cl::opt<std::string> TimerInterval(
    "timer-interval",
    cl::desc(
        "Minimum interval to check timers. "
        "Affects -max-time, -istats-write-interval, -stats-write-interval, and "
        "-uncovered-update-interval (default=1s)"),
    cl::init("1s"), cl::cat(TerminationCat));

llvm::cl::opt<bool> CheckTraversability(
    "check-traversability", cl::init(false),
    cl::desc("Check error trace for traversability (default=false)"));

llvm::cl::opt<bool> SmartResolveEntryFunction(
    "smart-resolve-entry-function", cl::init(false),
    cl::desc("Resolve entry function using code flow graph instead of taking "
             "function of first location (default=false)"));

cl::opt<unsigned long long>
    MaxCycles("max-cycles",
              cl::desc("stop execution after visiting some basic block this "
                       "amount of times (default=0)."),
              cl::init(0), cl::cat(TerminationCat));
} // namespace klee

void LocatedEventManager::prefetchFindFilename(const std::string &filename) {
  auto it = filenameCacheMap.find(filename);
  if (it != filenameCacheMap.end()) {
    filenameCache = it->second.get();
  } else {
    filenameCache = nullptr;
  }
}

bool LocatedEventManager::isInside(Location &loc,
                                   const klee::FunctionInfo &fi) {
  bool isInside = false;
  if (filenameCache == nullptr) {
    isInside = loc.isInside(fi);
    auto filenameCachePtr = std::make_unique<FilenameCache>();
    filenameCache = filenameCachePtr.get();
    filenameCacheMap.insert(
        std::make_pair(fi.file, std::move(filenameCachePtr)));
    filenameCache->insert(std::make_pair(loc.filename, isInside));
  } else {
    auto it = filenameCache->find(loc.filename);
    if (it == filenameCache->end()) {
      isInside = loc.isInside(fi);
      filenameCache->insert(std::make_pair(loc.filename, isInside));
    } else {
      isInside = it->second;
    }
  }
  return isInside;
}

TargetedHaltsOnTraces::TargetedHaltsOnTraces(ref<TargetForest> &forest) {
  auto leafs = forest->leafs();
  for (auto finalTargetSetPair : leafs) {
    traceToHaltTypeToConfidence.emplace(finalTargetSetPair.first,
                                        HaltTypeToConfidence());
  }
}

void TargetedHaltsOnTraces::subtractConfidencesFrom(
    TargetForest &forest, HaltExecution::Reason reason) {
  auto leafs = forest.leafs();
  for (auto finalTargetSetPair : leafs) {
    auto &haltTypeToConfidence =
        traceToHaltTypeToConfidence.at(finalTargetSetPair.first);
    auto confidence = finalTargetSetPair.second;
    auto it = haltTypeToConfidence.find(reason);
    if (it == haltTypeToConfidence.end()) {
      haltTypeToConfidence.emplace(reason, confidence);
    } else {
      haltTypeToConfidence[reason] = it->second + confidence;
    }
  }
}

void TargetedHaltsOnTraces::totalConfidenceAndTopContributor(
    const HaltTypeToConfidence &haltTypeToConfidence,
    confidence::ty *confidence, HaltExecution::Reason *reason) {
  *confidence = confidence::MaxConfidence;
  HaltExecution::Reason maxReason = HaltExecution::MaxTime;
  confidence::ty maxConfidence = confidence::MinConfidence;
  for (auto p : haltTypeToConfidence) {
    auto r = p.first;
    auto c = p.second;
    if (c > maxConfidence) {
      maxConfidence = c;
      maxReason = r;
    }
    *confidence -= c;
  }
  *reason = maxReason;
}

std::string
getAdviseWhatToIncreaseConfidenceRate(HaltExecution::Reason reason) {
  std::string what = "";
  switch (reason) {
  case HaltExecution::MaxSolverTime:
    what = MaxCoreSolverTime.ArgStr.str();
    break;
  case HaltExecution::MaxStackFrames:
    what = RuntimeMaxStackFrames.ArgStr.str();
    break;
  case HaltExecution::MaxTests:
    what = "max-tests"; // TODO: taken from run_klee.cpp
    break;
  case HaltExecution::MaxInstructions:
    what = MaxInstructions.ArgStr.str();
    break;
  case HaltExecution::MaxSteppedInstructions:
    what = MaxSteppedInstructions.ArgStr.str();
    break;
  case HaltExecution::MaxCycles:
    what = "max-cycles"; // TODO: taken from UserSearcher.cpp
    break;
  case HaltExecution::CovCheck:
    what = "cov-check"; // TODO: taken from StatsTracker.cpp
    break;
  case HaltExecution::MaxDepth:
    what = MaxDepth.ArgStr.str();
    break;
  case HaltExecution::ErrorOnWhichShouldExit: // this should never be the case
  case HaltExecution::ReachedTarget:          // this should never be the case
  case HaltExecution::NoMoreStates:           // this should never be the case
  case HaltExecution::Interrupt: // it is ok to advise to increase time if we
                                 // were interrupted by user
  case HaltExecution::MaxTime:
#ifndef ENABLE_KLEE_DEBUG
  default:
#endif
    what = MaxTime.ArgStr.str();
    break;
#ifdef ENABLE_KLEE_DEBUG
  default:
    what = std::to_string(reason);
    break;
#endif
  }
  return what;
}

void TargetedHaltsOnTraces::reportFalsePositives(bool canReachSomeTarget) {
  confidence::ty confidence;
  HaltExecution::Reason reason;
  for (const auto &targetSetWithConfidences : traceToHaltTypeToConfidence) {
    const auto &target = targetSetWithConfidences.first->getTargets().front();
    if (!target->shouldFailOnThisTarget())
      continue;
    bool atLeastOneReported = false;
    for (const auto &target : targetSetWithConfidences.first->getTargets()) {
      if (target->isReported) {
        atLeastOneReported = true;
        break;
      }
    }

    if (atLeastOneReported) {
      continue;
    }

    totalConfidenceAndTopContributor(targetSetWithConfidences.second,
                                     &confidence, &reason);
    if (canReachSomeTarget &&
        confidence::isVeryConfident(
            confidence)) { // We should not be so sure if there are states that
                           // can reach some targets
      confidence = confidence::VeryConfident;
      reason = HaltExecution::MaxTime;
    }
    reportFalsePositive(confidence, target->getErrors(), target->getId(),
                        getAdviseWhatToIncreaseConfidenceRate(reason));
    target->isReported = true;
  }
}

TargetedExecutionManager::LocationToBlocks
TargetedExecutionManager::prepareAllLocations(KModule *kmodule,
                                              Locations &locations) const {
  LocationToBlocks locToBlocks;
  LocatedEventManager lem;
  const auto &infos = kmodule->infos;
  for (const auto &kfunc : kmodule->functions) {
    const auto &fi = infos->getFunctionInfo(*kfunc->function);
    lem.prefetchFindFilename(fi.file);
    if (kmodule->origInfos.count(fi.file) == 0) {
      continue;
    }
    const auto &origInstsInFile = kmodule->origInfos.at(fi.file);
    for (auto it = locations.begin(); it != locations.end();) {
      auto loc = *it;
      if (locToBlocks.count(loc) != 0) {
        ++it;
        continue;
      }

      if (!lem.isInside(*loc, fi)) {
        ++it;
        continue;
      }
      Blocks blocks = Blocks();
      for (const auto &kblock : kfunc->blocks) {
        auto b = kblock.get();
        if (!loc->isInside(b, origInstsInFile)) {
          continue;
        }
        blocks.insert(b);
      }

      if (blocks.size() > 0) {
        locToBlocks[loc] = blocks;
        it = locations.erase(it);
      } else {
        ++it;
      }
    }
  }

  return locToBlocks;
}

TargetedExecutionManager::Locations
TargetedExecutionManager::collectAllLocations(const SarifReport &paths) const {
  Locations locations;
  for (const auto &res : paths.results) {
    for (const auto &loc : res.locations) {
      locations.insert(loc);
    }
  }

  return locations;
}

bool TargetedExecutionManager::canReach(const ref<Location> &from,
                                        const ref<Location> &to,
                                        LocationToBlocks &locToBlocks) const {
  for (auto fromBlock : locToBlocks[from]) {
    for (auto toBlock : locToBlocks[to]) {
      auto fromKf = fromBlock->parent;
      auto toKf = toBlock->parent;
      if (fromKf == toKf) {
        if (fromBlock == toBlock) {
          return true;
        }

        const auto &blockDist = codeGraphDistance.getDistance(fromBlock);
        if (blockDist.count(toBlock) != 0) {
          return true;
        }
      } else {
        const auto &funcDist = codeGraphDistance.getDistance(fromKf);
        if (funcDist.count(toKf) != 0) {
          return true;
        }

        const auto &backwardFuncDist =
            codeGraphDistance.getBackwardDistance(fromKf);
        if (backwardFuncDist.count(toKf) != 0) {
          return true;
        }
      }
    }
  }

  return false;
}

bool TargetedExecutionManager::tryResolveLocations(
    Result &result, LocationToBlocks &locToBlocks) const {
  std::vector<ref<Location>> resolvedLocations;
  size_t index = 0;
  for (const auto &location : result.locations) {
    auto it = locToBlocks.find(location);
    if (it != locToBlocks.end()) {
      if (!resolvedLocations.empty() && CheckTraversability) {
        if (!canReach(resolvedLocations.back(), location, locToBlocks)) {
          klee_warning("Trace %u is untraversable! Can't reach location %s "
                       "from location %s, so skipping this trace.",
                       result.id, location->toString().c_str(),
                       resolvedLocations.back()->toString().c_str());
          return false;
        }
      }
      resolvedLocations.push_back(location);
    } else if (index == result.locations.size() - 1) {
      klee_warning(
          "Trace %u is malformed! %s at location %s, so skipping this trace.",
          result.id, getErrorsString(result.errors).c_str(),
          location->toString().c_str());
      return false;
    }
    ++index;
  }

  result.locations = std::move(resolvedLocations);

  return true;
}

KFunction *TargetedExecutionManager::tryResolveEntryFunction(
    const Result &result, LocationToBlocks &locToBlocks) const {
  assert(result.locations.size() > 0);

  auto resKf = (*locToBlocks[result.locations[0]].begin())->parent;
  if (SmartResolveEntryFunction) {
    for (size_t i = 1; i < result.locations.size(); ++i) {
      const auto &funcDist = codeGraphDistance.getDistance(resKf);
      auto curKf = (*locToBlocks[result.locations[i]].begin())->parent;
      if (funcDist.count(curKf) == 0) {
        const auto &curFuncDist = codeGraphDistance.getDistance(curKf);
        if (curFuncDist.count(resKf) == 0) {
          klee_warning("Trace %u is malformed! Can't resolve entry function, "
                       "so skipping this trace.",
                       result.id);
          return nullptr;
        } else {
          resKf = curKf;
        }
      }
    }
  }

  return resKf;
}

std::map<KFunction *, ref<TargetForest>,
         TargetedExecutionManager::KFunctionLess>
TargetedExecutionManager::prepareTargets(KModule *kmodule, SarifReport paths) {
  Locations locations = collectAllLocations(paths);
  LocationToBlocks locToBlocks = prepareAllLocations(kmodule, locations);

  std::map<KFunction *, ref<TargetForest>, KFunctionLess> whitelists;

  for (auto &result : paths.results) {
    bool isFullyResolved = tryResolveLocations(result, locToBlocks);
    if (!isFullyResolved) {
      brokenTraces.insert(result.id);
      continue;
    }

    auto kf = tryResolveEntryFunction(result, locToBlocks);
    if (!kf) {
      brokenTraces.insert(result.id);
      continue;
    }

    if (whitelists.count(kf) == 0) {
      ref<TargetForest> whitelist = new TargetForest(kf);
      whitelists[kf] = whitelist;
    }
    whitelists[kf]->addTrace(result, locToBlocks);
  }

  return whitelists;
}

void TargetedExecutionManager::reportFalseNegative(ExecutionState &state,
                                                   ReachWithError error) {
  klee_warning("100.00%% %s False Negative at: %s", getErrorString(error),
               state.prevPC->getSourceLocation().c_str());
}

bool TargetedExecutionManager::reportTruePositive(ExecutionState &state,
                                                  ReachWithError error) {
  bool atLeastOneReported = false;
  for (auto target : state.targetForest.getTargets()) {
    if (!target->isThatError(error) || brokenTraces.count(target->getId()) ||
        reportedTraces.count(target->getId()))
      continue;

    /// The following code checks if target is a `call ...` instruction and we
    /// failed somewhere *inside* call
    auto possibleInstruction = state.prevPC;
    int i = state.stack.size() - 1;
    bool found = true;

    while (!target->isTheSameAsIn(
        possibleInstruction)) { // TODO: target->getBlock() ==
                                // possibleInstruction should also be checked,
                                // but more smartly
      if (i <= 0) {
        found = false;
        break;
      }
      possibleInstruction = state.stack.callStack().at(i).caller;
      i--;
    }
    if (!found)
      continue;

    state.error = error;
    atLeastOneReported = true;
    assert(!target->isReported);
    if (target->isThatError(ReachWithError::Reachable)) {
      klee_warning("100.00%% %s Reachable at trace %u", getErrorString(error),
                   target->getId());
    } else {
      klee_warning("100.00%% %s True Positive at trace %u",
                   getErrorString(error), target->getId());
    }
    target->isReported = true;
    reportedTraces.insert(target->getId());
  }
  return atLeastOneReported;
}

void TargetedExecutionManager::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
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

  TargetToStateUnorderedSetMap reachableStatesOfTarget;

  for (auto state : localStates) {
    auto &stateTargets = state->targets;

    for (auto target : stateTargets) {
      DistanceResult stateDistance = targetManager.distance(*state, target);
      switch (stateDistance.result) {
      case WeightResult::Miss:
        break;
      case WeightResult::Continue:
      case WeightResult::Done:
        reachableStatesOfTarget[target].insert(state);
        break;
      default:
        assert(0 && "unreachable");
      }
    }
  }

  for (auto state : localStates) {
    state->targetForest.divideConfidenceBy(reachableStatesOfTarget);
  }

  localStates.clear();
}