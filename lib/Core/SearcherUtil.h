// -*- C++ -*-
#pragma once

#include "ExecutionState.h"
#include "klee/Core/Path.h"
#include "ProofObligation.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace klee {

class ExecutionState;
class Executor;

struct BidirectionalAction {
friend class ref<BidirectionalAction>;
protected:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;
public:
  enum class Kind { Initialize, Forward, Branch, Backward, Terminate };

  BidirectionalAction() = default;
  virtual ~BidirectionalAction() = default;

  virtual Kind getKind() const = 0;

  static bool classof(const BidirectionalAction *) { return true; }
};

struct TerminateAction : public BidirectionalAction {
  friend class ref<TerminateAction>;

  TerminateAction() {}

  Kind getKind() const { return Kind::Terminate; }
  static bool classof(const BidirectionalAction *A) {
    return A->getKind() == Kind::Terminate;
  }
  static bool classof(const TerminateAction *) { return true; }
};

struct ForwardAction : public BidirectionalAction {
  friend class ref<ForwardAction>;

  ExecutionState *state;

  ForwardAction(ExecutionState *_state) : state(_state) {}

  Kind getKind() const { return Kind::Forward; }
  static bool classof(const BidirectionalAction *A) {
    return A->getKind() == Kind::Forward || A->getKind() == Kind::Branch;
  }
  static bool classof(const ForwardAction *) { return true; }
};

struct BranchAction : public ForwardAction {
  friend class ref<BranchAction>;

  ExecutionState *state;

  BranchAction(ExecutionState *_state) : ForwardAction(_state) {}

  Kind getKind() const { return Kind::Branch; }
  static bool classof(const BidirectionalAction *A) {
    return A->getKind() == Kind::Branch;
  }
  static bool classof(const BranchAction *) { return true; }
};

struct BackwardAction : public BidirectionalAction {
  friend class ref<BackwardAction>;

  ExecutionState *state;
  ProofObligation *pob;

  BackwardAction(ExecutionState *_state, ProofObligation *_pob)
    : state(_state), pob(_pob) {}

  Kind getKind() const { return Kind::Backward; }
  static bool classof(const BidirectionalAction *A) {
    return A->getKind() == Kind::Backward;
  }
  static bool classof(const BackwardAction *) { return true; }
};

struct InitializeAction : public BidirectionalAction {
  friend class ref<InitializeAction>;

  KInstruction *location;
  std::set<Target> targets;

  InitializeAction(KInstruction *_location, std::set<Target> _targets)
    : location(_location), targets(_targets) {}

  Kind getKind() const { return Kind::Initialize; }
  static bool classof(const BidirectionalAction *A) {
    return A->getKind() == Kind::Initialize;
  }
  static bool classof(const InitializeAction *) { return true; }
};

struct SearcherConfig {
  Executor *executor;
};


struct Conflict {
  using constraint_ty = std::pair<ref<Expr>, KInstruction *>;
  using core_ty = std::vector<constraint_ty>;

  Path path;
  core_ty core;

  Conflict() = default;
  Conflict(const Path &_path, const core_ty &_core) :
    path(_path), core(_core) {}
};

struct TargetedConflict {
friend class ref<TargetedConflict>;
private:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

public:

  Conflict conflict;
  KBlock *target;

  TargetedConflict(Conflict &_conflict, KBlock *_target) :
    conflict(_conflict), target(_target) {}
};


struct ActionResult {
  friend class ref<ActionResult>;
protected:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;
public:
  enum class Kind { Initialize, Forward, Branch, Backward, Terminate };

  ActionResult() = default;
  virtual ~ActionResult() = default;

  virtual Kind getKind() const = 0;

  static bool classof(const ActionResult *) { return true; }
};

struct ForwardResult : ActionResult {
  friend class ref<ForwardResult>;

  ExecutionState *current;
  const std::vector<ExecutionState *> &addedStates;
  const std::vector<ExecutionState *> &removedStates;
  ref<TargetedConflict> targetedConflict;

  ForwardResult(ExecutionState *_s, const std::vector<ExecutionState *> &_a,
                const std::vector<ExecutionState *> &_r,
                ref<TargetedConflict> _c = ref<TargetedConflict>())
    : current(_s), addedStates(_a), removedStates(_r), targetedConflict(_c) {};

  Kind getKind() const { return Kind::Forward; }
  static bool classof(const ActionResult *A) {
    return A->getKind() == Kind::Forward;
  }
  static bool classof(const ForwardResult *) { return true; }
};

struct BranchResult : ActionResult {
  friend class ref<BranchResult>;

  ExecutionState *current;
  const std::vector<ExecutionState *> &addedStates;
  const std::vector<ExecutionState *> &removedStates;

  BranchResult(ExecutionState *_s, const std::vector<ExecutionState *> &a,
               const std::vector<ExecutionState *> &r)
    : current(_s), addedStates(a), removedStates(r) {};

  Kind getKind() const { return Kind::Branch; }
  static bool classof(const ActionResult *A) {
    return A->getKind() == Kind::Branch;
  }
  static bool classof(const BranchResult *) { return true; }
};

struct BackwardResult : ActionResult {
  friend class ref<BackwardResult>;

  std::vector<ProofObligation*> newPobs;
  ExecutionState *state;
  ProofObligation *oldPob;

  BackwardResult(std::vector<ProofObligation*> _newPobs, ExecutionState *_state, ProofObligation *_oldPob)
    : newPobs(_newPobs), state(_state), oldPob(_oldPob) {}

  Kind getKind() const { return Kind::Backward; }
  static bool classof(const ActionResult *A) {
    return A->getKind() == Kind::Backward;
  }
  static bool classof(const BackwardResult *) { return true; }
};

struct InitializeResult : ActionResult {
  friend class ref<InitializeResult>;

  KInstruction *location;
  ExecutionState &state;

  InitializeResult(KInstruction *_loc, ExecutionState &es) :
    location(_loc), state(es) {}

  Kind getKind() const { return Kind::Initialize; }
  static bool classof(const ActionResult *A) {
    return A->getKind() == Kind::Initialize;
  }
  static bool classof(const InitializeResult *) { return true; }
};

struct TerminateResult : ActionResult {
  friend class ref<TerminateResult>;

  Kind getKind() const { return Kind::Terminate; }
  static bool classof(const ActionResult *A) {
    return A->getKind() == Kind::Terminate;
  }
  static bool classof(const TerminateResult *) { return true; }
};

class Ticker {
  std::vector<unsigned> ticks;
  size_t index = 0;
  unsigned counter = 0;

public:
  Ticker(std::vector<unsigned> ticks) : ticks(ticks) {}

  unsigned getCurrent() {
    unsigned current = index;
    counter += 1;
    if (counter == ticks[index]) {
      index = (index + 1) % ticks.size();
      counter = 0;
    }
    return current;
  }

  void moveToNext() {
    if (counter != 0) {
      index = (index + 1) % ticks.size();
      counter = 0;
    }
  }
};

}
