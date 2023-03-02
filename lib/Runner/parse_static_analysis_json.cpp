#include "klee/Runner/run_klee.h"

/* -*- mode: c++; c-basic-offset: 2; -*- */

//===-- main.cpp ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Support/ErrorHandling.h"

#include <klee/Misc/json.hpp>
#include <fstream>

using json = nlohmann::json;

using namespace llvm;
using namespace klee;


class Frames {

using FramesMtd = std::vector<std::pair<std::string, std::string> >;
FramesMtd frames;
std::string functionShouldBeCalled = "";
unsigned id;

bool checkConsistency(const std::string &filename, const std::string &function) {
  bool ok = true;
  if (frames.empty())
    return ok;
  if (functionShouldBeCalled.empty())
    functionShouldBeCalled = frames.back().second;
  if (functionShouldBeCalled != function) {
    klee_warning("JSON inconsistency in trace %u: trace event has function %s but should have function %s. Skipping trace %u",
      id, function.c_str(), functionShouldBeCalled.c_str(), id);
      ok = false;
  } else {
    frames.emplace_back(filename, function);
  }
  functionShouldBeCalled = "";
  return ok;
}

bool ensureAtLeastOneFrame(json &loc) {
  std::string filename = loc.at("file");
  std::string function = loc.at("function");
  bool ok = checkConsistency(filename, function);
  if (frames.empty())
    frames.emplace_back(filename, function);
  return ok;
}

public:

explicit Frames(unsigned id) : id(id) {}

bool enterCall(json &loc, const std::string &function) {
  bool ok = ensureAtLeastOneFrame(loc);
  functionShouldBeCalled = function;
  return ok;
}

bool exitCall(json &loc, const std::string &function) {
  if (!frames.empty()) {
    auto oldFunc = frames.back().second;
    if (function == oldFunc) {
      frames.pop_back();
    }
  }
  return ensureAtLeastOneFrame(loc);
}

bool handleNonCallEvent(json &loc) {
  return ensureAtLeastOneFrame(loc);
}

FramesMtd::iterator entryPoint() {
  return frames.begin();
}

}; // class Frames

class TraceParser {

struct cmpLocatedEvent {
    bool operator()(const LocatedEvent& a, const LocatedEvent& b) const {
        return *a.getLocation() < *b.getLocation();
    }
};

std::map<LocatedEvent, LocatedEvent *, cmpLocatedEvent> locatedEvents;

LocatedEvent *locatedEvent(LocatedEvent *event) {
  if (event->shouldFailOnThisTarget())
    return event;
  auto it = locatedEvents.find(*event);
  if (it == locatedEvents.end()) {
    locatedEvents.insert(std::make_pair(*event, event));
    return event;
  } else {
    delete event;
    return it->second;
  }
}

ReachWithError parseError(json &traceEvent) {
  if (!traceEvent.contains("event"))
    return ReachWithError::None;
  auto event = traceEvent.at("event");
  if (event.empty() || event.at("kind") != "Error")
    return ReachWithError::None;
  std::string errorType = event.at("error").at("sort");
  if ("NullDereference" == errorType)
    return ReachWithError::NullPointerException;
  else if ("CheckAfterDeref" == errorType)
    return ReachWithError::NullCheckAfterDerefException;
  else if ("DoubleFree" == errorType)
    return ReachWithError::DoubleFree;
  else if ("UseAfterFree" == errorType)
    return ReachWithError::UseAfterFree;
  else // TODO: extent to new types
    return ReachWithError::None;
}

LocatedEvent *parseTraceEvent(json &traceEvent, unsigned id, bool last = false) {
  auto location = traceEvent.at("location");
  std::string file = location.at("file");
  auto error = parseError(traceEvent);
  unsigned reportLine = location.at("reportLine");
  Location *loc = nullptr;
  if (location.contains("function") && location.contains("column") &&
      location.contains("instructionOpcode")) {
    std::string function = location.at("function");
    unsigned column = location.at("column");
    unsigned instructionOpcode = location.at("instructionOpcode");
    unsigned offset = 0;
    if (location.contains("instructionOffset")) {
      offset = location.at("instructionOffset");
    }
    loc = new Location(file, function, reportLine, offset, column, instructionOpcode);
  } else {
    loc = new Location(file, reportLine);
  }
  auto le = new LocatedEvent(loc, error, id);
  if (last) {
    le->setErrorReachableIfNone();
  }
  return locatedEvent(le);
}

LocatedEvent *deduceEntryPoint(json &trace, unsigned id) {
  Frames frames(id);
  for (auto traceEvent : trace) {
    auto loc = traceEvent.at("location");
    auto event = traceEvent.at("event");
    auto eventKind = event.at("kind");
    bool ok = true;
    if ("ExitCall" == eventKind) {
      ok = frames.exitCall(loc, event.at("function"));
    } else if ("EnterCall" == eventKind) {
      ok = frames.enterCall(loc, event.at("function"));
    } else {
      ok = frames.handleNonCallEvent(loc);
    }
    if (!ok)
      return nullptr;
  }
  auto p = frames.entryPoint();
  auto le = new LocatedEvent(new Location(p->first, p->second), id);
  // llvm::errs() << "Deduced entry point: " << p->first << ' ' << p->second << '\n';
  return locatedEvent(le);
}

std::vector<LocatedEvent *> *parseTrace(json &trace, unsigned id) {
  auto entryPoint = deduceEntryPoint(trace, id);
  if (!entryPoint)
    return nullptr;
  auto out = new std::vector<LocatedEvent *>();
  out->reserve(trace.size() + 1);
  out->push_back(entryPoint);
  for (unsigned i = 0; i < trace.size() - 1; ++i) {
    auto le = parseTraceEvent(trace[i], id);
    out->push_back(le);
  }
  if (trace.size() > 0) {
    auto le = parseTraceEvent(trace.back(), id, true);
    out->push_back(le);
  }
  return out;
}

public:
TraceParser() {}

PathForest *parseErrors(json &errors) {
  auto layer = new PathForest();
  if (errors.empty())
    return layer;
  for (auto errorTrace : errors) {
    unsigned id = errorTrace.at("id");
    auto trace = parseTrace(errorTrace.at("trace"), id);
    if (!trace)
      continue;
    layer->addTrace(trace);
    delete trace;
  }
  return layer;
}

}; // class TraceParser

PathForest *parseInputPathTree(const std::string &inputPathTreePath) {
  std::ifstream file(inputPathTreePath);
  if (file.fail())
    klee_error("Cannot read file %s", inputPathTreePath.c_str());
  json pathForestJSON = json::parse(file);
  TraceParser tp;
  return tp.parseErrors(pathForestJSON.at("errors"));
}
