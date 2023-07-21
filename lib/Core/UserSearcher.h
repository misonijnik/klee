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

#include "llvm/Support/CommandLine.h"

namespace klee {
class BackwardSearcher;
class Executor;
class Searcher;

// XXX gross, should be on demand?
bool userSearcherRequiresMD2U();

void initializeSearchOptions();

Searcher *constructUserSearcher(Executor &executor,
                                bool stopAfterReachingTarget = true);

BackwardSearcher *constructUserBackwardSearcher();
} // namespace klee

#endif /* KLEE_USERSEARCHER_H */
