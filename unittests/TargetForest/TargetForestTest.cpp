//===-- RefTest.cpp ---------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include "Core/TargetForest.h"
#include "gtest/gtest.h"

using namespace klee;
using klee::TargetForest;

TEST(TargetForestTest, DoubleStep) {
  //TODO: [Yurii Kostyukov]
  // std::unordered_map<KBlock *, ref<Target>> block2target;
  // ResolvedLocations rl;
  // std::unordered_set<KBlock *> blocks = {(KBlock*)1, (KBlock*)2, (KBlock*)3};
  // rl.locations.emplace_back(nullptr, blocks);
  // block2target[(KBlock*)1] = (Target*)10;
  // block2target[(KBlock*)2] = (Target*)20;
  // block2target[(KBlock*)3] = (Target*)30;
  // TargetForest whitelists({rl}, block2target);
  // auto first = whitelists.getDebugReferenceCount();
  // EXPECT_EQ(1u, first);
  // auto tmp = whitelists;
  // auto second = whitelists.getDebugReferenceCount();
  // EXPECT_EQ(2u, second);
  // auto tmp2 = whitelists;
  // auto third = whitelists.getDebugReferenceCount();
  // EXPECT_EQ(3u, third);
  // tmp.debugStepToRandomLoc();
  // auto forth = whitelists.getDebugReferenceCount();
  // EXPECT_EQ(2u, forth);
  // tmp2.debugStepToRandomLoc();
  // auto fifth = whitelists.getDebugReferenceCount();
  // EXPECT_EQ(1u, fifth);
}
