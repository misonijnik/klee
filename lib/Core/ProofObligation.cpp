#include "ProofObligation.h"
#include "ExecutionState.h"

namespace klee {

unsigned ProofObligation::counter = 0;

void ProofObligation::addCondition(ref<Expr> e, KInstruction *loc, bool *sat) {
  ConstraintManager c(condition);
  c.addConstraint(e, loc, sat);
}

void ProofObligation::detachParent() {
  if (parent) {
    parent->children.erase(this);
  }
}

std::string ProofObligation::print() const {
  std::string ret;
  std::string s;
  llvm::raw_string_ostream ss(s);
  location->basicBlock->printAsOperand(ss, false);
  ret += "Proof Obligation at " + ss.str();
  ret += " id: " + std::to_string(id) + '\n';
  ret += "The conditions are:\n";
  if(condition.empty()) ret += "None\n";
  for(auto i : condition) {
    ret += i->toString() + '\n';
  }
  ret += "Children: ";
  if(children.empty()) ret += "None";
  for(auto i : children) {
    ret += std::to_string(i->id) + " ";
  }
  return ret;
}

ProofObligation *propagateToReturn(ProofObligation *pob,
                                   KInstruction *callSite,
                                   KBlock *returnBlock) {
  ProofObligation *ret = new ProofObligation(pob);
  ret->location = returnBlock;
  ret->condition.remove_first();
  ret->condition.prepend(returnBlock);
  ret->stack.push_back(callSite);
  return ret;
}

} // namespace klee
