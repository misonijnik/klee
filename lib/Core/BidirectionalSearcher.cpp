//===-- BidirectionalSearcher.h ----------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "BidirectionalSearcher.h"
#include "BackwardSearcher.h"
#include "ExecutionState.h"
#include "Executor.h"
#include "ForwardSearcher.h"
#include "Initializer.h"
#include "MergeHandler.h"
#include "ProofObligation.h"
#include "SearcherUtil.h"
#include "UserSearcher.h"
#include "klee/Core/Interpreter.h"
#include "klee/Module/KModule.h"
#include "klee/Support/ErrorHandling.h"
#include "klee/Support/OptionCategories.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Instructions.h"

#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

#include <cstdlib>

namespace {
llvm::cl::opt<bool> DebugBidirectionalSearcher("debug-bidirectional-searcher",
                                               llvm::cl::desc(""),
                                               llvm::cl::init(false),
                                               llvm::cl::cat(klee::DebugCat));

llvm::cl::opt<unsigned>
    MaxCycles("max-cycles",
              llvm::cl::desc("stop execution after visiting some basic block "
                             "this amount of times (default=0)."),
              llvm::cl::init(0), llvm::cl::cat(klee::TerminationCat));

llvm::cl::opt<bool> DoBackwardFirst(
    "do-backward-first",
    llvm::cl::desc("do all possible backward steps with a state before "
                   "forward step (default=true)."),
    llvm::cl::init(true), llvm::cl::cat(klee::ExecCat));

llvm::cl::opt<bool> PruneStates(
    "prune-states",
    llvm::cl::desc("prune a state of failed backward step (default=false)."),
    llvm::cl::init(false), llvm::cl::cat(klee::ExecCat));

} // namespace

namespace klee {

BidirectionalSearcher::StepKind BidirectionalSearcher::selectStep() {
  size_t initial_choice = ticker.getCurrent();
  size_t choice = initial_choice;

  do {
    switch (choice) {
    case 0: {
      if (!forward->empty()) {
        return StepKind::Forward;
      }
      break;
    }
    case 1: {
      if (!branch->empty() && !pobs.empty()) {
        return StepKind::Branch;
      }
      break;
    }
    case 2: {
      if (!backward->empty()) {
        return StepKind::Backward;
      }
      break;
    }
    case 3: {
      if (!initializer->empty()) {
        return StepKind::Initialize;
      }
      break;
    }
    }
    ticker.moveToNext();
    choice = ticker.getCurrent();
  } while (choice != initial_choice);

  return StepKind::Terminate;
}

ref<BidirectionalAction> BidirectionalSearcher::selectAction() {
  ref<BidirectionalAction> action;
  while (action.isNull()) {
    switch (selectStep()) {

    case StepKind::Forward: {
      auto &state = forward->selectState();

      if (DoBackwardFirst && state.backwardStepsLeftCounter > 0) {
        break;
      }

      if (PruneStates && state.failedBackwardStepsCounter > 0) {
        forward->update(nullptr, {}, {&state});
        ex->pauseState(state);
        break;
      }

      if (isStuck(state)) {
        KBlock *target = ex->calculateTargetByBlockHistory(state);
        if (target) {
          state.targets.insert(Target(target));
          forward->update(&state, {}, {});
          action = new ForwardAction(&state);
        } else {
          forward->update(nullptr, {}, {&state});
          ex->pauseState(state);
        }
      } else
        action = new ForwardAction(&state);
      break;
    }

    case StepKind::Branch: {
      auto &state = branch->selectState();
      KInstruction *prevKI = state.prevPC;
      if (ex->initialState->getInitPCBlock() != state.getInitPCBlock() &&
          prevKI->inst->isTerminator() &&
          state.multilevel.count(state.getPCBlock()) > 0) {
        branch->update(nullptr, {}, {&state});
        ex->pauseState(state);
      } else {
        action = new BranchAction(&state);
      }
      break;
    }

    case StepKind::Backward: {
      auto pobState = backward->selectAction();
      action = new BackwardAction(pobState.second, pobState.first);
      break;
    }

    case StepKind::Initialize: {
      auto initAndTargets = initializer->selectAction();
      action =
          new InitializeAction(initAndTargets.first, initAndTargets.second);
      break;
    }

    case StepKind::Terminate: {
      action = new TerminateAction();
      break;
    }
    }
  }
  return action;
}

void BidirectionalSearcher::updateForward(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates,
    ref<TargetedConflict> targetedConflict) {

  forward->update(current, addedStates, removedStates);

  std::vector<ExecutionState *> states;
  if (current)
    states.push_back(current);
  states.insert(states.end(), addedStates.begin(), addedStates.end());

  for (auto &state : states) {
    if (state->getPrevPCBlock() != state->getPCBlock() &&
        !isa<KReturnBlock>(state->pc->parent) &&
        !isa<KReturnBlock>(state->prevPC->parent)) {
      Target target = Target(state->pc->parent);
      backward->addState(target, state);
    }
  }

  if (targetedConflict) {
    if (!rootBlocks.count(targetedConflict->target->basicBlock) &&
        !reachableBlocks.count(targetedConflict->target->basicBlock)) {
      rootBlocks.insert(targetedConflict->target->basicBlock);
      initializer->addConflictInit(targetedConflict->conflict,
                                   targetedConflict->target);
      ProofObligation *pob = new ProofObligation(targetedConflict->target);

      if (DebugBidirectionalSearcher) {
        llvm::errs() << "Add new proof obligation.\n";
        llvm::errs() << "At: " << pob->location->getIRLocation() << "\n";
        llvm::errs() << "\n";
      }
      addPob(pob);
    }
  }
}

void BidirectionalSearcher::updateBranch(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  std::map<Target, std::unordered_set<ExecutionState *>> reached;

  branch->update(current, addedStates, removedStates, reached);

  for (auto &targetStates : reached) {
    for (auto state : targetStates.second) {
      if (targetStates.first.atReturn() && state->stack.size() > 0)
        continue;
      if (DebugBidirectionalSearcher) {
        llvm::errs() << "New isolated state.\n";
        llvm::errs() << "Id: " << state->id << "\n";
        llvm::errs() << "Path: " << state->constraints.getPath() << "\n";
        llvm::errs() << "Constraints:\n" << state->constraints;
        llvm::errs() << "\n";
      }
      backward->addState(targetStates.first, state);
    }
  }
}

void BidirectionalSearcher::updateBackward(
    std::vector<ProofObligation *> newPobs, ProofObligation *oldPob) {
  for (auto pob : newPobs) {
    addPob(pob);
  }
}

void BidirectionalSearcher::updateInitialize(KInstruction *location,
                                             ExecutionState &state) {
  branch->update(nullptr, {&state}, {});
}

void BidirectionalSearcher::update(ref<ActionResult> r) {
  switch (r->getKind()) {
  case ActionResult::Kind::Forward: {
    auto fr = cast<ForwardResult>(r);
    updateForward(fr->current, fr->addedStates, fr->removedStates,
                  fr->targetedConflict);
    break;
  }
  case ActionResult::Kind::Branch: {
    auto brr = cast<BranchResult>(r);
    updateBranch(brr->current, brr->addedStates, brr->removedStates);
    break;
  }
  case ActionResult::Kind::Backward: {
    auto bckr = cast<BackwardResult>(r);
    updateBackward(bckr->newPobs, bckr->oldPob);
    if (bckr->state->backwardStepsLeftCounter > 0) {
      --bckr->state->backwardStepsLeftCounter;
      if (bckr->newPobs.empty())
        ++bckr->state->failedBackwardStepsCounter;
    }
    break;
  }
  case ActionResult::Kind::Initialize: {
    auto ir = cast<InitializeResult>(r);
    updateInitialize(ir->location, ir->state);
    break;
  }
  default:
    break;
  }
}

BidirectionalSearcher::BidirectionalSearcher(const SearcherConfig &cfg)
    : ticker({80, 10, 5, 5}) {
  ex = cfg.executor;
  forward = new GuidedSearcher(constructUserSearcher(*cfg.executor), true);
  branch = new GuidedSearcher(
      std::unique_ptr<ForwardSearcher>(new BFSSearcher()), false);
  backward = new RecencyRankedSearcher(MaxCycles);
  initializer = new ConflictCoreInitializer(ex->initialState->pc);
}

BidirectionalSearcher::~BidirectionalSearcher() {
  for (auto pob : pobs) {
    delete pob;
  }
  pobs.clear();
  delete forward;
  delete branch;
  delete backward;
  delete initializer;
}

void BidirectionalSearcher::closeProofObligation(ProofObligation *pob) {
  answerPob(pob);
  std::queue<ProofObligation *> pobs;
  pobs.push(pob->root ? pob->root : pob);
  while (pobs.size()) {
    ProofObligation *currPob = pobs.front();
    pobs.pop();
    for (auto child : currPob->children) {
      pobs.push(child);
    }
    removePob(currPob);
    delete currPob;
  }
}

bool isStuck(ExecutionState &state) {
  KInstruction *prevKI = state.prevPC;
  return prevKI->inst->isTerminator() && state.targets.empty() &&
         state.multilevel.count(state.getPCBlock()) > MaxCycles;
}

void BidirectionalSearcher::addPob(ProofObligation *pob) {
  pobs.push_back(pob);
  backward->update(pob);
  initializer->addPob(pob);
}

void BidirectionalSearcher::removePob(ProofObligation *pob) {
  auto pos = std::find(pobs.begin(), pobs.end(), pob);
  if (pos != pobs.end()) {
    pobs.erase(pos);
  }
  backward->removePob(pob);
  initializer->removePob(pob);
}

void BidirectionalSearcher::answerPob(ProofObligation *pob) {
  while (pob) {
    reachableBlocks.insert(pob->location->basicBlock);
    pob = pob->parent;
  }
}

ref<BidirectionalAction> ForwardOnlySearcher::selectAction() {
  if (searcher->empty()) {
    return new TerminateAction();
  }
  return new ForwardAction(&searcher->selectState());
}

void ForwardOnlySearcher::update(ref<ActionResult> r) {
  switch (r->getKind()) {
  case ActionResult::Kind::Forward: {
    auto fr = cast<ForwardResult>(r);
    searcher->update(fr->current, fr->addedStates, fr->removedStates);
    break;
  }
  case ActionResult::Kind::Terminate: {
    break;
  }
  default: {
    klee_error("ForwardOnlySearcher received non-forward action result.");
  }
  }
}

void ForwardOnlySearcher::closeProofObligation(ProofObligation *) {}

ForwardOnlySearcher::ForwardOnlySearcher(const SearcherConfig &cfg) {
  searcher = constructUserSearcher(*cfg.executor);
}

ForwardOnlySearcher::~ForwardOnlySearcher() {}

ref<BidirectionalAction> GuidedOnlySearcher::selectAction() {
  if (searcher->empty()) {
    return new TerminateAction();
  }

  ref<BidirectionalAction> action;
  while (action.isNull()) {
    auto &state = searcher->selectState();
    if (isStuck(state)) {
      KBlock *target = ex->calculateTargetByBlockHistory(state);
      if (target) {
        state.targets.insert(Target(target));
        searcher->update(&state, {}, {});
        action = new ForwardAction(&state);
      } else {
        searcher->update(nullptr, {}, {&state});
        ex->pauseState(state);
      }
    } else
      action = new ForwardAction(&state);
  }
  return new ForwardAction(&searcher->selectState());
}

void GuidedOnlySearcher::update(ref<ActionResult> r) {
  switch (r->getKind()) {
  case ActionResult::Kind::Forward: {
    auto fr = cast<ForwardResult>(r);
    searcher->update(fr->current, fr->addedStates, fr->removedStates);
    break;
  }
  case ActionResult::Kind::Terminate: {
    break;
  }
  default: {
    klee_error("ForwardOnlySearcher received non-forward action result.");
  }
  }
}

void GuidedOnlySearcher::closeProofObligation(ProofObligation *) {}

GuidedOnlySearcher::GuidedOnlySearcher(const SearcherConfig &cfg) {
  searcher = std::unique_ptr<GuidedSearcher>(
      new GuidedSearcher(constructUserSearcher(*cfg.executor), true));
  ex = cfg.executor;
}

GuidedOnlySearcher::~GuidedOnlySearcher() {}

} // namespace klee
