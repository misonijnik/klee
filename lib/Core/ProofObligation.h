#pragma once

#include "Memory.h"

#include "klee/Core/Path.h"
#include "klee/Expr/Constraints.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace klee {

class ExecutionState;
class MemoryObject;

class ProofObligation {

private:
  static unsigned counter;

public:
  const unsigned id;
  ProofObligation *parent;
  ProofObligation *root;
  std::unordered_set<ProofObligation *> children;
  std::vector<KInstruction *> stack;
  std::map<ExecutionState *, unsigned> propagationCount;

  KBlock *location;
  PathConstraints condition;

  std::vector<std::pair<ref<const MemoryObject>, const Array *>> sourcedSymbolics;

  ProofObligation(KBlock *_location, ProofObligation *_parent = nullptr)
      : id(counter++), parent(_parent), root(_parent ? _parent->root : this),
        stack(_parent ? _parent->stack : std::vector<KInstruction *>()),
        propagationCount(_parent ? _parent->propagationCount
                                 : std::map<ExecutionState *, unsigned>()),
        location(_location), condition(_location) {
    if (parent) {
      parent->children.insert(this);
    }
  }

  explicit ProofObligation(ProofObligation *pob)
      : id(counter++), parent(pob->parent), root(pob->root), stack(pob->stack),
        propagationCount(pob->propagationCount),
        location(pob->location), condition(pob->condition) {
    if (parent) {
      parent->children.insert(this);
    }
  }

  // no copy ctor
  ProofObligation(const ProofObligation &state) = delete;
  // no copy assignment
  ProofObligation &operator=(const ProofObligation &) = delete;
  // no move ctor
  ProofObligation(ProofObligation &&) noexcept = delete;
  // no move assignment
  ProofObligation& operator=(ProofObligation &&) noexcept = delete;

  ~ProofObligation() = default;

  bool isOriginPob() const noexcept { return !parent; }
  void addCondition(ref<Expr> e, KInstruction *loc, bool *sat = 0);
  void detachParent();
  bool atReturn() const { return isa<KReturnBlock>(location); }
  std::string print() const;
};

struct ProofObligationIDCompare {
  bool operator()(const ProofObligation *a, const ProofObligation *b) const {
    return a->id < b->id;
  }
};

ProofObligation *propagateToReturn(ProofObligation *pob, KInstruction *callSite,
                                   KBlock *returnBlock);

} // namespace klee
