#include "Initializer.h"
#include "ExecutionState.h"
#include "klee/Core/Path.h"
#include "ProofObligation.h"
#include "SearcherUtil.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Solver/Solver.h"
#include "klee/Support/ModuleUtil.h"
#include "klee/Support/OptionCategories.h"

#include "llvm/IR/Instructions.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stack>
#include <utility>

namespace {
llvm::cl::opt<bool> DebugInitializer(
    "debug-initializer",
    llvm::cl::desc(""),
    llvm::cl::init(false),
    llvm::cl::cat(klee::DebugCat));
}

namespace klee {

std::pair<KInstruction *, std::set<Target>>
ConflictCoreInitializer::selectAction() {
  auto action = conflictCoreInits.front();
  conflictCoreInits.pop();
  return action;
}

bool ConflictCoreInitializer::empty() {
  return conflictCoreInits.empty();
}

void ConflictCoreInitializer::addPob(ProofObligation *pob) {
  Target target(pob->location);
  if (mapTargetToInitLocations.count(target)) {
    for (auto &init : mapTargetToInitLocations[target]) {
      if (!mapInitLocationToTargets.count(init))
        continue;

      conflictCoreInits.push(std::make_pair(init, mapInitLocationToTargets[init]));
      for (auto &sideTarget : mapInitLocationToTargets[init]) {
        if (sideTarget == target)
          continue;

        auto initPos =  std::find(
          mapTargetToInitLocations[sideTarget].begin(),
          mapTargetToInitLocations[sideTarget].end(),
          init);
        mapTargetToInitLocations[sideTarget].erase(initPos);
      }
      mapInitLocationToTargets.erase(init);
    }
    mapTargetToInitLocations.erase(target);
  }
}

void ConflictCoreInitializer::removePob(ProofObligation *pob) {}

void ConflictCoreInitializer::addConflictInit(const Conflict &conflict, KBlock *target) {
  const Conflict::core_ty &core = conflict.core;
  assert(!core.empty());
  const Path &path = conflict.path;
  std::set<std::pair<KInstruction *, Target>> inits;
  KFunction *mainKF = initInst->parent->parent;
  KInstruction *current = initInst;
  std::set<KFunction *> visited;
  for (size_t pathIndex = 1; pathIndex < path.size(); pathIndex++) {
    if (path.getBlock(pathIndex)->parent != path.getBlock(pathIndex - 1)->parent) {
      if (path.getBlock(pathIndex - 1)->parent == mainKF) {
        auto dismantled = dismantle(current->parent, {path.getBlock(pathIndex - 1)});
        for (auto &blockpair : dismantled) {
          inits.insert(std::make_pair(blockpair.first == current->parent ? current : blockpair.first->instructions[0],
                                      Target(blockpair.second)));
          if (blockpair.second->getKBlockType() == KBlockType::Call) {
            KFunction* f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
            if (f) {
              inits.insert(std::make_pair(blockpair.second->instructions[0],
                                          Target(f->entryKBlock)));
            }
          }
        }
        inits.insert(std::make_pair(path.getBlock(pathIndex - 1)->instructions[0],
                                    Target(path.getBlock(pathIndex))));
        current =
          isa<llvm::CallInst>(path.getBlock(pathIndex - 1)->instructions[0]->inst) ?
            path.getBlock(pathIndex - 1)->instructions[1] :
            path.getBlock(pathIndex)->instructions[0];
      }
      if (path.getBlock(pathIndex)->parent != mainKF &&
          !dismantledKFunctions.count(path.getBlock(pathIndex)->parent)) {
        visited.insert(path.getBlock(pathIndex)->parent);
      }
    }
    if (pathIndex == path.size() - 1) {
      if (path.getBlock(pathIndex)->parent == mainKF &&
          path.getBlock(pathIndex) != current->parent) {
        auto dismantled = dismantle(current->parent, {path.getBlock(pathIndex)});
        for (auto &blockpair : dismantled) {
          inits.insert(std::make_pair(
            blockpair.first == current->parent ? current : blockpair.first->instructions[0],
            Target(blockpair.second)));
          if (blockpair.second->getKBlockType() == KBlockType::Call) {
            KFunction *f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
            if (f) {
              inits.insert(std::make_pair(blockpair.second->instructions[0],
                                          Target(f->entryKBlock)));
            }
          }
        }
      }
    }
  }
  for (auto kf : visited) {
    dismantledKFunctions.insert(kf);
    std::vector<KBlock *> to = kf->returnKBlocks;

    if (kf == path.getFinalBlock()->parent)
      to.push_back(path.getFinalBlock());

    auto dismantled = dismantle(kf->entryKBlock, to);
    for (auto blockpair : dismantled) {

      inits.insert(std::make_pair(blockpair.first->instructions[0],
                                  Target(blockpair.second)));

      if (blockpair.second->getKBlockType() == KBlockType::Call) {
        KFunction *f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
        if (f) {
          inits.insert(std::make_pair(blockpair.second->instructions[0],
                                      Target(f->entryKBlock)));
        }
      }
    }
  }
  inits.insert(std::make_pair(path.getFinalBlock()->instructions[0], Target(target)));

  std::map<KInstruction *, std::set<Target>> ret;
  if (DebugInitializer) {
    llvm::errs() << "Initialization of entry points and targets: \n";
  }
  for (auto &init : inits) {
    if (!initialized[init.first].count(init.second) &&
        !(isa<KReturnBlock>(init.second.block) && init.first->parent == init.second.block)) {
      if (DebugInitializer) {
        llvm::errs() << init.first->getIRLocation() << "\n" << init.first->getSourceLocation() << "\n";
        llvm::errs() << init.second.print() << "\n" << init.second.block->instructions[0]->getSourceLocation();
        llvm::errs() << "\n";
      }
      ret[init.first].insert(init.second);
      initialized[init.first].insert(init.second);
    }
  }
  if (DebugInitializer) {
    llvm::errs() << "\n";
  }
  for (auto &initTargets : ret) {
    mapInitLocationToTargets[initTargets.first] = initTargets.second;
    for (auto &target : initTargets.second) {
      mapTargetToInitLocations[target].push_back(initTargets.first);
    }
  }
}

};
