//===-- UserSearcher.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_USERSEARCHER_H
#define KLEE_USERSEARCHER_H
#include "BidirectionalSearcher.h"

#include <memory>

namespace klee {
class BackwardSearcher;
class Executor;
class Searcher;

// XXX gross, should be on demand?
bool userSearcherRequiresMD2U();

void initializeSearchOptions();

std::unique_ptr<Searcher> constructBaseSearcher(Executor &executor);
std::unique_ptr<Searcher> constructUserSearcher(Executor &executor);

std::unique_ptr<BackwardSearcher>
constructUserBackwardSearcher(Executor &executor);

std::unique_ptr<BidirectionalSearcher> constructUserBidirectionalSearcher(
    Executor &executor, std::unique_ptr<IsolatedStatesInitializer> initializer);

struct BaseSearcherConstructor {
  Executor &executor;
  BaseSearcherConstructor(Executor &executor) : executor(executor) {}
  Searcher *operator()() const {
    return constructBaseSearcher(executor).release();
  }
};
} // namespace klee

#endif /* KLEE_USERSEARCHER_H */
