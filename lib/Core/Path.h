// -*- C++ -*-
#pragma once
#include "klee/Module/KModule.h"
#include "llvm/IR/Instructions.h"
#include <functional>
#include <string>
#include <vector>

namespace klee {

struct Path {
  friend Path concat(const Path& lhs, const Path& rhs);

public:

  KBlock* getInitialBlock() const;
  KBlock* getFinalBlock() const;

  KBlock* getBlock(size_t index) const;

  void append(KBlock* bb) {
    path.push_back(bb);
  }

  void prepend(KBlock* bb) {
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
  explicit Path(std::vector<KBlock*> path) : path(path) {}

private:
  std::vector<KBlock *> path;
};

Path concat(const Path& lhs, const Path& rhs);

Path parse(std::string str, KModule *m);
};
