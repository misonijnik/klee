// -*- C++ -*-
#ifndef KLEE_SEARCHERUTIL_H
#define KLEE_SEARCHERUTIL_H

#include "ExecutionState.h"
#include "ProofObligation.h"
#include "klee/Expr/Path.h"
#include "klee/Module/KInstruction.h"

namespace klee {

struct Propagation {
  ExecutionState *state;
  ProofObligation *pob;

  Propagation(ExecutionState *_state, ProofObligation *_pob)
      : state(_state), pob(_pob) {}

  bool operator==(const Propagation &rhs) const {
    return state == rhs.state && pob == rhs.pob;
  }

  bool operator<(const Propagation &rhs) const {
    return std::make_tuple(state->id, pob->id) <
           std::make_tuple(rhs.state->id, rhs.pob->id);
  }
};

struct PropagationIDCompare {
  bool operator()(const Propagation &a, const Propagation &b) const {
    return std::make_tuple(a.state->getID(), a.pob->getID()) <
           std::make_tuple(b.state->getID(), b.pob->getID());
  }
};

using propagations_ty = std::set<Propagation, PropagationIDCompare>;

struct SearcherAction {
  friend class ref<SearcherAction>;

protected:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

public:
  enum class Kind { Initialize, Forward, Backward };

  SearcherAction() = default;
  virtual ~SearcherAction() = default;

  virtual Kind getKind() const = 0;

  static bool classof(const SearcherAction *) { return true; }
};

struct ForwardAction : public SearcherAction {
  friend class ref<ForwardAction>;

  ExecutionState *state;

  ForwardAction(ExecutionState *_state) : state(_state) {}

  Kind getKind() const { return Kind::Forward; }
  static bool classof(const SearcherAction *A) {
    return A->getKind() == Kind::Forward;
  }
  static bool classof(const ForwardAction *) { return true; }
};

struct BackwardAction : public SearcherAction {
  friend class ref<BackwardAction>;

  Propagation prop;

  BackwardAction(Propagation prop) : prop(prop) {}

  Kind getKind() const { return Kind::Backward; }
  static bool classof(const SearcherAction *A) {
    return A->getKind() == Kind::Backward;
  }
  static bool classof(const BackwardAction *) { return true; }
};

struct InitializeAction : public SearcherAction {
  friend class ref<InitializeAction>;

  KInstruction *location;
  std::set<ref<Target>> targets;

  InitializeAction(KInstruction *_location, std::set<ref<Target>> _targets)
      : location(_location), targets(_targets) {}

  Kind getKind() const { return Kind::Initialize; }
  static bool classof(const SearcherAction *A) {
    return A->getKind() == Kind::Initialize;
  }
  static bool classof(const InitializeAction *) { return true; }
};

} // namespace klee

#endif
