//===-- Target.cpp --------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Target.h"

using namespace klee;

std::string Target::print() const {
  std::string repr = "Target: ";
  repr += block->getAssemblyLocation();
  if (atReturn()) {
    repr += " (at the end)";
  }
  return repr;
}
