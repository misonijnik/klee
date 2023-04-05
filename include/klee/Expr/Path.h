#ifndef KLEE_PATH_H
#define KLEE_PATH_H

#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include <stack>
#include <vector>

namespace klee {

class Path {
  using path_ty = std::vector<KBlock *>;

public:
  struct PathIndex {
    unsigned long block;
    unsigned long instruction;
  };

  struct BlockRange {
    unsigned long first;
    unsigned long last;
  };

  void advance(KInstruction *ki);

  friend bool operator==(const Path &lhs, const Path &rhs) {
    return lhs.KBlocks == rhs.KBlocks &&
           lhs.firstInstruction == rhs.firstInstruction &&
           lhs.lastInstruction == rhs.lastInstruction;
  }
  friend bool operator!=(const Path &lhs, const Path &rhs) {
    return !(lhs == rhs);
  }

  unsigned KBlockSize() const;
  const path_ty &getBlocks() const;
  unsigned getFirstIndex() const;
  unsigned getLastIndex() const;

  PathIndex getCurrentIndex() const;

  std::stack<KInstruction *> getStack() const;

  std::vector<std::pair<KFunction *, BlockRange>> asFunctionRanges() const;

  static Path concat(const Path &l, const Path &r);

private:
  path_ty KBlocks;
  // Index of the first instruction in the first basic block
  unsigned firstInstruction;
  // Index of the last (current) instruction in the current basic block
  unsigned lastInstruction;
};

}; // namespace klee

#endif
