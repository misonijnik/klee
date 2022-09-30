// -*- C++ -*-
#pragma once
#include "klee/ADT/Ref.h"
#include "klee/Module/KModule.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace klee {

struct Path {
  friend Path concat(const Path& lhs, const Path& rhs);
  friend class ref<Path>;

private:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

public:
  KBlock *getInitialBlock() const;
  KBlock *getFinalBlock() const;
  KBlock *getBlock(size_t index) const;

  void append(KBlock *bb) {
    path.push_back(bb);
  }

  void prepend(KBlock *bb) {
    path.insert(path.begin(), bb);
  }

  size_t getCurrentIndex() const {
    return path.size() - 1;
  }

  size_t size() const {
    return path.size();
  }

  bool empty() const {
    return path.empty();
  }

  std::set<KFunction *> getFunctionsInPath() const;

  std::string toString() const;

  friend bool operator==(const Path& lhs, const Path& rhs) {
    return lhs.path == rhs.path;
  }
  friend bool operator!=(const Path& lhs, const Path& rhs) {
    return lhs.path != rhs.path;
  }

  friend bool operator<(const Path& lhs, const Path& rhs) {
    return lhs.path < rhs.path;
  }

  Path() = default;
  explicit Path(std::vector<KBlock *> path) : path(path) {}

private:
  std::vector<KBlock *> path;
};

Path concat(const Path& lhs, const Path& rhs);

ref<Path> parse(std::string str, KModule *m,
                          const std::map<std::string, size_t> &DBHashMap);
};
