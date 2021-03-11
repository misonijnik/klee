//===-- PForest.cpp ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PForest.h"

#include "ExecutionState.h"

#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprPPrinter.h"
#include "klee/Support/OptionCategories.h"

#include <bitset>
#include <vector>

using namespace klee;
using namespace llvm;

PForest::PForest() {}

void PForest::addRoot(ExecutionState *initialState) {
  PTree *tree = new PTree(initialState);
  trees[tree->getID()] = tree;
}

void PForest::attach(PTreeNode *node, ExecutionState *leftState,
                     ExecutionState *rightState) {
  assert(trees.find(node->getTreeID()) != trees.end());
  trees[node->getTreeID()]->attach(node, leftState, rightState);
}

void PForest::remove(PTreeNode *node) {
  assert(trees.find(node->getTreeID()) != trees.end());
  trees[node->getTreeID()]->remove(node);
}

void PForest::dump(llvm::raw_ostream &os) {
  for (auto ntree : trees)
    ntree.second->dump(os);
}
