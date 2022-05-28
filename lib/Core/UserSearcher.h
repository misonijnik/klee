/*
 * This source file has been modified by Yummy Research Team. Copyright (c) 2022
 */

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

#include <memory>

namespace klee {
  class Executor;
  class Searcher;

  // XXX gross, should be on demand?
  bool userSearcherRequiresMD2U();

  void initializeSearchOptions();

  std::unique_ptr<Searcher> constructUserSearcher(Executor &executor);
}

#endif /* KLEE_USERSEARCHER_H */
