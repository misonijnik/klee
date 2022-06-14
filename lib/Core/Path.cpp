#include "Path.h"
#include "klee/Module/KModule.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <vector>
#include <stack>

using namespace klee;

KBlock* Path::getInitialBlock() const {
  return path.front();
}

KBlock* Path::getFinalBlock() const {
  return path.back();
}

KBlock* Path::getBlock(size_t index) const {
  return path[index];
}

std::string Path::toString() const {
  unsigned stackCount = 0;
  std::string repr = "";
  for (size_t i = 0; i<path.size(); i++) {
    if (i == 0 || (path[i-1]->parent != path[i]->parent &&
                   path[i-1]->getKBlockType() == KBlockType::Call)) {
      repr += "(";
      repr += path[i]->parent->function->getName();
      repr += ": ";
      stackCount++;
    }
    repr += path[i]->getLabel() + " ";
    if (i == path.size() - 1 || (path[i]->parent != path[i + 1]->parent &&
                                 path[i]->getKBlockType() != KBlockType::Call)) {
      repr.pop_back();
      repr += ") ";
      if (stackCount == 0) {
        std::string tmp = "(";
        tmp += path[i]->parent->function->getName();
        tmp += ": ";
        repr = tmp + repr;
      } else {
        stackCount--;
      }
    }
  }
  repr.pop_back();
  repr += std::string(")", stackCount);
  return repr;
}


Path klee::concat(const Path& lhs, const Path& rhs) {
  if(lhs.empty()) {
    return rhs;
  }
  if(rhs.empty()) {
    return lhs;
  }
  assert(lhs.getFinalBlock() == rhs.getInitialBlock() && "Paths are not compatible.");
  std::vector<KBlock*> path;
  path.reserve(lhs.path.size() + rhs.path.size() - 1);
  path.insert(path.end(), lhs.path.begin(), lhs.path.end());
  path.insert(path.end(), rhs.path.begin() + 1, rhs.path.end());
  return Path(std::move(path));
}

Path klee::parse(std::string str, KModule *m) {
  std::stack<KFunction *> functionStack;
  std::vector<KBlock *> path;
  size_t index = 0;
  while (true) {
    while (index < str.size() && str[index] == ' ') {
      ++index;
    }
    if (index == str.size())
      break;
    if (str[index] == '(') {
      ++index;
      std::string functionName;
      while (str[index] != ':') {
        functionName += str[index];
        ++index;
      }
      functionStack.push(m->functionNameMap[functionName]);
      ++index;
    } else if (str[index] == ')') {
      ++index;
      functionStack.pop();
    } else if (str[index] == '%') {
      std::string label = "%";
      ++index;
      while (str[index] != ' ' && str[index] != ')') {
        label += str[index];
        ++index;
      }
      path.push_back(functionStack.top()->labelMap[label]);
    }
  }
  return Path(path);
}
