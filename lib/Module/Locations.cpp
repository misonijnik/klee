//===-- Locations.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Module/Locations.h"
#include "klee/Module/KModule.h"
#include "klee/Module/KInstruction.h"
#include "klee/Support/ErrorHandling.h"
#include "llvm/IR/Module.h"

#include <sstream>

using namespace klee;
using namespace llvm;


KFunction *Location::getFunction(KModule *module, std::ostringstream &error) const {
  assert(hasFunctionWithOffset());
  auto f = module->module->getFunction(function);
  if (f) {
    auto kf = module->functionMap[f];
    if (kf)
      return kf;
  }
  error << "Cannot resolve function " << function << " in llvm bitcode";
  return nullptr;
}

std::set<KInstruction *> Location::initInstructions(KModule *origModule, KModule *kleeModule, std::ostringstream &error) {
  // resolve column
  auto origKF = getFunction(origModule, error);
  if (!origKF)
    return instructions;

  // resolve real instruction
  auto kf = getFunction(kleeModule, error);
  if (!kf)
    return instructions;
  auto n = kf->numInstructions;

  if (line == 0 && column == 0 && instructionOpcode == 0) {
    instructions.insert(kf->instructions[0]);
    return instructions;
  }

  KInstruction *last_match = nullptr;
  for (unsigned start = 0, finish = n; last_match == nullptr; start = 0, finish = offset) {
  for (unsigned i = start; i < finish; i++) {
    auto new_match = kf->instructions[i];
    auto info = new_match->info;
    if (info->line == line && info->column == column && new_match->inst->getOpcode() == instructionOpcode) {
      last_match = new_match;
      continue;
    }
    if (last_match != nullptr) {
      instructions.insert(last_match);
    }
  }
  if (start == 0)
    break;
  }
  assert(last_match != nullptr);
  return instructions;
}

bool Location::isTheSameAsIn(KInstruction *instr) const {
  return instr->info->line == line;
}

bool isOSSeparator(char c) {
  return c == '/' || c == '\\';
}

bool Location::isInside(const FunctionInfo &info) const {
  size_t suffixSize = 0;
  int m = info.file.size() - 1, n = filename.size() - 1;
  for (;
       m >= 0 && n >= 0 && info.file[m] == filename[n];
       m--, n--) {
    suffixSize++;
    if (isOSSeparator(filename[n]))
      return true;
  }
  return suffixSize >= 3 && (n == -1 ? (m == -1 || isOSSeparator(info.file[m])) : (m == -1 && isOSSeparator(filename[n])));
}

std::string Location::toString() const {
  if (!instructions.empty()) {
    auto instruction = *instructions.begin();
    return instruction->getSourceLocation();
  }
  std::ostringstream out;
  out << filename << ':' << line;
  return out.str();
}

bool Location::isInside(KBlock *block) const {
  auto first = block->getFirstInstruction()->info->line;
  if (first > line)
    return false;
  auto last = block->getLastInstruction()->info->line;
  return line <= last; // and `first <= line` from above
}

int Location::compare(const Location &other) const {
  if (line < other.line)
    return -1;
  if (line > other.line)
    return 1;
  if (offset < other.offset)
    return -1;
  if (offset > other.offset)
    return 1;
  if (filename < other.filename)
    return -1;
  if (filename > other.filename)
    return 1;
  if (function < other.function)
    return -1;
  if (function > other.function)
    return 1;
  return 0;
}

bool Location::equals(const Location &other) const {
  return compare(other) == 0;
}

int LocatedEvent::compare(const LocatedEvent &other) const {
  if (error != other.error) {
    return error < other.error ? -1 : 1;
  }
  if (id != other.id) {
    return id < other.id ? -1 : 1;
  }
  if (location.isNull()) {
    return other.location.isNull() ? 0 : 1;
  } else if (other.location.isNull()) {
    return -1;
  }
  if (*location != *other.location) {
    return (*location < *other.location) ? -1 : 1;
  }
  return 0;
}

bool LocatedEvent::operator<(const LocatedEvent &other) const {
  return
      error < other.error    || (error == other.error &&
  (location < other.location || (location == other.location &&
        (id < other.id))));
}

bool LocatedEvent::operator==(const LocatedEvent &other) const {
  return error == other.error && location == other.location && id == other.id;
}

void LocatedEvent::setErrorReachableIfNone() {
  if (shouldFailOnThisTarget())
    return;
  error = ReachWithError::Reachable;
}

std::string LocatedEvent::toString() const {
  return location->toString();
}

void PathForest::addSubTree(LocatedEvent * loc, PathForest *subTree) {
  layer.insert(std::make_pair(loc, subTree));
}

void PathForest::addLeaf(LocatedEvent * loc) {
  addSubTree(loc, new PathForest());
}

void PathForest::addTrace(std::vector<LocatedEvent *> *trace) {
  auto forest = this;
  for (auto event : *trace) {
    auto it = forest->layer.find(event);
    if (it == forest->layer.end()) {
      auto next = new PathForest();
      forest->layer.insert(std::make_pair(event, next));
      forest = next;
    } else {
      forest = it->second;
    }
  }
}

bool PathForest::empty() const {
  return layer.empty();
}

void PathForest::normalize() {
  for (auto &p : layer) {
    auto child = p.second;
    if (child == nullptr)
      child = new PathForest();
    if (!child->empty())
      continue;
    child->addLeaf(p.first);
  }
}

PathForest::~PathForest() {
  for (auto p : layer) {
    if (p.second != nullptr)
      delete p.second;
  }
}
