//===-- BidirectionalSearcher.h ---------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#pragma once
#include "BackwardSearcher.h"
#include "Executor.h"
#include "SearcherUtil.h"
#include "Initializer.h"
#include "ProofObligation.h"
#include "ForwardSearcher.h"
#include "klee/Module/KModule.h"
#include <memory>
#include <unordered_set>
#include <vector>


namespace klee {

class IBidirectionalSearcher {
public:
  virtual ref<BidirectionalAction> selectAction() = 0;
  virtual void update(ref<ActionResult>) = 0;
  virtual void closeProofObligation(ProofObligation *) = 0;
  virtual ~IBidirectionalSearcher() {}
};


class BidirectionalSearcher : public IBidirectionalSearcher {
public:
  ref<BidirectionalAction> selectAction() override;
  void update(ref<ActionResult>) override;
  void closeProofObligation(ProofObligation *) override;
  explicit BidirectionalSearcher(const SearcherConfig &);
  ~BidirectionalSearcher() override;

private:
  enum class StepKind { Initialize, Forward, Branch, Backward, Terminate };

  Executor *ex; // hack

  GuidedSearcher *forward;
  GuidedSearcher *branch;
  RecencyRankedSearcher *backward;
  ConflictCoreInitializer *initializer;

  std::vector<ProofObligation *> pobs;

  Ticker ticker;

  // Temporary _-_
  std::unordered_set<llvm::BasicBlock *> rootBlocks;
  std::unordered_set<llvm::BasicBlock *> reachableBlocks;

  StepKind selectStep();
  void updateForward(ExecutionState *current,
                     const std::vector<ExecutionState *> &addedStates,
                     const std::vector<ExecutionState *> &removedStates,
                     ref<TargetedConflict> targetedConflict);
  void updateBranch(ExecutionState *current,
                    const std::vector<ExecutionState *> &addedStates,
                    const std::vector<ExecutionState *> &removedStates);
  void updateBackward(std::vector<ProofObligation*> newPobs, ProofObligation *oldPob);
  void updateInitialize(KInstruction *location, ExecutionState &state);

  void addPob(ProofObligation *);
  void removePob(ProofObligation *);
  void answerPob(ProofObligation *);
  bool isStuck(ExecutionState &);
};

class ForwardOnlySearcher : public IBidirectionalSearcher {
  public:
  ref<BidirectionalAction> selectAction() override;
  void update(ref<ActionResult>) override;
  void closeProofObligation(ProofObligation *) override;
  explicit ForwardOnlySearcher(const SearcherConfig &);
  ~ForwardOnlySearcher() override;

private:
  std::unique_ptr<ForwardSearcher> searcher;
};

} // namespace klee
