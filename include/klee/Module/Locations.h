//===-- Locations.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Classes to represent code locations
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_LOCATIONS_H
#define KLEE_LOCATIONS_H

#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Module/InstructionInfoTable.h"
#include "klee/ADT/Ref.h"


namespace klee {

enum ReachWithError {
  DoubleFree = 0,
  UseAfterFree,
  NullPointerException,
  NullCheckAfterDerefException,
  Reachable,
  None,
};

static const char *ReachWithErrorNames[] = {"DoubleFree",
                                            "UseAfterFree",
                                            "NullPointerException",
                                            "NullCheckAfterDerefException",
                                            "Reachable",
                                            "None",};

class Location {
  std::set<KInstruction *> instructions;
  unsigned line;
  unsigned offset;
  unsigned column;
  unsigned instructionOpcode;

  KFunction *getFunction(KModule *module, std::ostringstream &error) const;
public:
  const std::string function;
  const std::string filename;

  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

  Location(const std::string &f, const std::string &function, unsigned line, unsigned offset, unsigned column, unsigned instructionOpcode)
    : line(line), offset(offset), column(column), instructionOpcode(instructionOpcode), function(function), filename(f) {}
  Location(const std::string &f, const std::string &function)
    : Location(f, function, 0, 0, 0, 0) {}
  Location(const std::string &filename, unsigned line)
    : Location(filename, "", line, 0, 0, 0) {}

  bool isTheSameAsIn(KInstruction *instr) const;
  bool isInside(const FunctionInfo &info) const;
  bool isInside(KBlock *block) const;

  std::string toString() const;

  bool hasFunctionWithOffset() const {
    return !function.empty();
  }

  std::set<KInstruction *> initInstructions(KModule *origModule, KModule *kleeModule, std::ostringstream &error);

  int compare(const Location &other) const;
  bool equals(const Location &other) const;

  friend bool operator <(const Location& lhs, const Location& rhs) {
    return lhs.compare(rhs) < 0;
  }

  friend bool operator==(const Location &lhs, const Location &rhs) {
    return lhs.equals(rhs);
  }

  friend bool operator!=(const Location &lhs, const Location &rhs) {
    return !(lhs == rhs);
  }
};

struct LocatedEvent {
private:
  ref<Location> location;
  ReachWithError error;
  unsigned id;

public:
  LocatedEvent(Location *location, ReachWithError error, unsigned id)
    : location(location), error(error), id(id) {}

  LocatedEvent(Location *location, unsigned id)
    : LocatedEvent(location, ReachWithError::None, id) {}

  LocatedEvent(Location *location)
    : LocatedEvent(location, 0) {}
  
  explicit LocatedEvent() : LocatedEvent(nullptr) {}

  bool hasFunctionWithOffset() {
    return location->hasFunctionWithOffset();
  }

  int compare(const LocatedEvent &other) const;
  bool operator<(const LocatedEvent &other) const;
  bool operator==(const LocatedEvent &other) const;
  ReachWithError getError() const { return error; }
  const char *getErrorString() const { return ReachWithErrorNames[error]; }
  unsigned getId() const { return id; }
  ref<Location> getLocation() const { return location; }
  bool shouldFailOnThisTarget() const { return error != ReachWithError::None; }
  void setErrorReachableIfNone();

  std::string toString() const;
};

struct PathForest {
  std::unordered_map<LocatedEvent *, PathForest *> layer;
  ~PathForest();

  void addSubTree(LocatedEvent *loc, PathForest *subTree);
  void addLeaf(LocatedEvent *loc);

  void addTrace(std::vector<LocatedEvent *> *trace);

  bool empty() const;

  /// @brief Sets singleton paths to size two
  void normalize();
};

} // End klee namespace

#endif /* KLEE_LOCATIONS_H */
