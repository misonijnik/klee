#include "ProofObligation.h"
#include "ExecutionState.h"

namespace klee {

size_t ProofObligation::counter = 0;

void ProofObligation::addCondition(ref<Expr> e, std::optional<size_t> loc, bool *sat) {
  ConstraintManager c(condition);
  c.addConstraint(e, loc, sat);
}

std::string ProofObligation::print() {
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

} // namespace klee
