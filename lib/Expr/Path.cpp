#include "klee/Expr/Path.h"

#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"

using namespace klee;
using namespace llvm;

void Path::advance(KInstruction *ki) {
  if (KBlocks.empty()) {
    firstInstruction = ki->index;
    lastInstruction = ki->index;
    KBlocks.push_back(ki->parent);
    return;
  }
  auto lastBlock = KBlocks.back();
  if (ki->parent != lastBlock) {
    KBlocks.push_back(ki->parent);
  }
  lastInstruction = ki->index;
  return;
}

unsigned Path::KBlockSize() const { return KBlocks.size(); }

const Path::path_ty &Path::getBlocks() const { return KBlocks; }

unsigned Path::getFirstIndex() const { return firstInstruction; }

unsigned Path::getLastIndex() const { return lastInstruction; }

Path::PathIndex Path::getCurrentIndex() const {
  return {KBlocks.size() - 1, lastInstruction};
}

std::stack<KInstruction *> Path::getStack() const {
  std::stack<KInstruction *> stack;
  for (unsigned i = 0; i < KBlocks.size(); i++) {
    auto current = KBlocks[i];
    if (i != 0 && KBlocks[i - 1]->parent != current->parent) {
      assert(isa<KCallBlock>(current));
      stack.pop();
      continue;
    }
    if (isa<KCallBlock>(current)) {
      stack.push(current->instructions[0]);
    }
  }
  return stack;
}

std::vector<std::pair<KFunction *, Path::BlockRange>>
Path::asFunctionRanges() const {
  assert(!KBlocks.empty());
  std::vector<std::pair<KFunction *, BlockRange>> ranges;
  BlockRange range{0, 0};
  KFunction *function = KBlocks[0]->parent;
  for (unsigned i = 0; i < KBlocks.size(); i++) {
    if (KBlocks[i]->parent == function) {
      if (i == KBlocks.size() - 1) {
        range.last = i;
        ranges.push_back({function, range});
        return ranges;
      } else {
        continue;
      }
    }
    range.last = i - 1;
    ranges.push_back({function, range});
    range.first = i;
    function = KBlocks[i]->parent;
  }
  llvm_unreachable("asFunctionRanges reached the end of the for!");
}

Path Path::concat(const Path &l, const Path &r) {
  Path path = l;
  for (auto block : r.KBlocks) {
    path.KBlocks.push_back(block);
  }
  path.lastInstruction = r.lastInstruction;
  return path;
}
