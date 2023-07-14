//===-- DistanceCalculator.h ------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_DISTANCE_CALCULATOR_H
#define KLEE_DISTANCE_CALCULATOR_H

#include "ExecutionState.h"

namespace llvm {
class BasicBlock;
} // namespace llvm

namespace klee {
class CodeGraphDistance;
struct Target;

enum WeightResult : std::uint8_t {
  Done = 0U,
  Continue = 1U,
  Miss = 2U,
};

using weight_type = unsigned;

struct DistanceResult {
  WeightResult result;
  weight_type weight;
  bool isInsideFunction;

  explicit DistanceResult(WeightResult result_ = WeightResult::Miss,
                          weight_type weight_ = 0,
                          bool isInsideFunction_ = true)
      : result(result_), weight(weight_), isInsideFunction(isInsideFunction_){};

  bool operator<(const DistanceResult &b) const;

  std::string toString() const;
};

class DistanceCalculator {
public:
  explicit DistanceCalculator(CodeGraphDistance &codeGraphDistance_)
      : codeGraphDistance(codeGraphDistance_) {}

  DistanceResult getDistance(const ExecutionState &es, ref<Target> target);

  DistanceResult getDistance(const KInstruction *prevPC, const KInstruction *pc,
                             const ExecutionStack::call_stack_ty &frames,
                             ReachWithError error, ref<Target> target);

private:
  enum TargetKind : std::uint8_t {
    LocalTarget = 0U,
    PreTarget = 1U,
    PostTarget = 2U,
    NoneTarget = 3U,
  };

  struct SpeculativeState {
  private:
    unsigned hashValue;

    unsigned computeHash();

  public:
    const KInstruction *pc;
    TargetKind kind;
    ReachWithError error;
    SpeculativeState(const KInstruction *pc_, TargetKind kind_,
                     ReachWithError error_)
        : pc(pc_), kind(kind_), error(error_) {
      computeHash();
    }
    ~SpeculativeState() = default;
    unsigned hash() const { return hashValue; }
  };

  struct SpeculativeStateHash {
    bool operator()(const SpeculativeState &a) const { return a.hash(); }
  };

  struct SpeculativeStateCompare {
    bool operator()(const SpeculativeState &a,
                    const SpeculativeState &b) const {
      return a.pc == b.pc && a.error == b.error && a.kind == b.kind;
    }
  };

  using SpeculativeStateToDistanceResultMap =
      std::unordered_map<SpeculativeState, DistanceResult, SpeculativeStateHash,
                         SpeculativeStateCompare>;
  using TargetToSpeculativeStateToDistanceResultMap =
      std::unordered_map<ref<Target>, SpeculativeStateToDistanceResultMap,
                         TargetHash, TargetCmp>;

  using StatesSet = std::unordered_set<ExecutionState *>;

  CodeGraphDistance &codeGraphDistance;
  TargetToSpeculativeStateToDistanceResultMap distanceResultCache;
  StatesSet localStates;

  DistanceResult getDistance(const KInstruction *pc, TargetKind kind,
                             ReachWithError error, ref<Target> target);

  DistanceResult computeDistance(const KInstruction *pc, TargetKind kind,
                                 ReachWithError error,
                                 ref<Target> target) const;

  bool distanceInCallGraph(KFunction *kf, KBlock *kb, unsigned int &distance,
                           const std::unordered_map<KFunction *, unsigned int>
                               &distanceToTargetFunction,
                           ref<Target> target) const;
  WeightResult tryGetLocalWeight(const KInstruction *pc, weight_type &weight,
                                 const std::vector<KBlock *> &localTargets,
                                 ref<Target> target) const;
  WeightResult
  tryGetPreTargetWeight(const KInstruction *pc, weight_type &weight,
                        const std::unordered_map<KFunction *, unsigned int>
                            &distanceToTargetFunction,
                        ref<Target> target) const;
  WeightResult tryGetTargetWeight(const KInstruction *pc, weight_type &weight,
                                  ref<Target> target) const;
  WeightResult tryGetPostTargetWeight(const KInstruction *pc,
                                      weight_type &weight,
                                      ref<Target> target) const;
};
} // namespace klee

#endif /* KLEE_DISTANCE_CALCULATOR_H */
