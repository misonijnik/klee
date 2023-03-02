//===-- TargetedExecutionReporter.h ------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TARGETEDEXECUTIONREPORTER_H
#define KLEE_TARGETEDEXECUTIONREPORTER_H

#include "klee/Module/Locations.h"

namespace klee {

namespace confidence {
  using ty = double;
  static ty MinConfidence = 0.0;
  static ty MaxConfidence = 100.0;
  static ty Confident = 90.0;
  static ty VeryConfident = 99.0;
  bool isConfident(ty conf);
  bool isVeryConfident(ty conf);
  bool isNormal(ty conf);
  std::string toString(ty conf);
  ty min(ty left, ty right);
};

void reportFalsePositive(
  confidence::ty confidence,
  LocatedEvent &event,
  std::string whatToIncrease);

}

#endif
