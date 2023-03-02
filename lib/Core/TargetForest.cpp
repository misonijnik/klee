//===-- TargetForest.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TargetForest.h"
#include "klee/Core/TargetedExecutionReporter.h"
#include "klee/Expr/Expr.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"

using namespace klee;
using namespace llvm;

void TargetForest::Layer::pathForestToTargetForest(
  TargetForest::Layer *self,
  PathForest *pathForest,
  std::unordered_map<LocatedEvent *, TargetsSet *> &loc2Targets,
  std::unordered_set<unsigned> &broken_traces) {
  if (pathForest == nullptr)
    return;
  for (auto &p : pathForest->layer) {
    auto it = loc2Targets.find(p.first);
    if (it == loc2Targets.end()) {
      continue;
    }
    auto targets = it->second;
    for (auto &target : *targets) {
      if (broken_traces.count(target->getId()))
        continue;
      ref<TargetForest::Layer> next = new TargetForest::Layer();
      pathForestToTargetForest(next.get(), p.second, loc2Targets, broken_traces);
      self->insert(target, next);
    }
  }
}

TargetForest::Layer::Layer(PathForest *pathForest, std::unordered_map<LocatedEvent *, TargetsSet *> &loc2Targets, std::unordered_set<unsigned> &broken_traces)
  : Layer() {
  pathForestToTargetForest(this, pathForest, loc2Targets, broken_traces);
}

TargetForest::TargetForest(
    PathForest *pathForest,
    std::unordered_map<LocatedEvent *, TargetsSet *> &loc2Targets,
    std::unordered_set<unsigned> &broken_traces, KFunction *entryFunction)
    : TargetForest(
          new TargetForest::Layer(pathForest, loc2Targets, broken_traces),
          entryFunction) {}

void TargetForest::Layer::propagateConfidenceToChildren() {
  auto parent_confidence = getConfidence();
  for (auto &kv : forest) {
    kv.second->confidence = kv.second->getConfidence(parent_confidence);
  }
}

void TargetForest::Layer::unionWith(TargetForest::Layer *other) {
  if (other->forest.empty())
    return;
  other->propagateConfidenceToChildren();
  for (const auto &kv : other->forest) {
    auto it = forest.find(kv.first);
    if (it == forest.end()) {
      forest.insert(std::make_pair(kv.first, kv.second));
      continue;
    }
    auto layer = new Layer(it->second);
    layer->unionWith(kv.second.get());
    it->second = layer;
  }
}

void TargetForest::Layer::block(ref<Target> target) {
  if (empty())
    return;
  auto res = forest.find(target);
  if (res != forest.end()) {
    forest.erase(res);
  }
  for (InternalLayer::iterator itf = forest.begin(), eitf = forest.end();
       itf != eitf;) {
    ref<Layer> layer = itf->second->blockLeaf(target);
    itf->second = layer;
    if (itf->second->empty()) {
      itf = forest.erase(itf);
      eitf = forest.end();
    } else {
      ++itf;
    }
  }
}

bool TargetForest::Layer::deepFind(ref<Target> target) const {
  if (empty())
    return false;
  auto res = forest.find(target);
  if (res != forest.end()) {
    return true;
  }
  for (auto &f : forest) {
    if (f.second->deepFind(target))
      return true;
  }
  return false;
}

bool TargetForest::Layer::deepFindIn(ref<Target> child, ref<Target> target) const {
  auto res = forest.find(child);
  if (res == forest.end()) {
    return false;
  }
  return res->second->deepFind(target);
}

TargetForest::Layer *TargetForest::Layer::removeChild(ref<Target> child) const {
  auto result = new Layer(this);
  result->forest.erase(child);
  return result;
}

TargetForest::Layer *TargetForest::Layer::addChild(ref<Target> child) const {
  auto result = new Layer(this);
  result->forest.insert({child, new Layer()});
  return result;
}

TargetForest::Layer *
TargetForest::Layer::blockLeafInChild(ref<Target> child,
                                      ref<Target> leaf) const {
  auto subtree = forest.find(child);
  assert(subtree != forest.end());
  if (subtree->second->forest.empty()) {
    if (subtree->first != leaf) {
      return new Layer(this);
    } else {
      return removeChild(child);
    }
  }

  ref<Layer> sublayer = new Layer(subtree->second);
  sublayer->block(leaf);
  if (sublayer->empty()) {
    return removeChild(child);
  } else {
    InternalLayer subforest;
    subforest[child] = sublayer;
    sublayer = new Layer(subforest, confidence);
    auto result = replaceChildWith(child, sublayer.get());
    return result;
  }
}

TargetForest::Layer *TargetForest::Layer::blockLeaf(ref<Target> leaf) const {
  auto result = new Layer(this);
  if (forest.empty()) {
    return result;
  }

  for (auto &layer : forest) {
    result = ref<Layer>(result)->blockLeafInChild(layer.first, leaf);
  }
  return result;
}

TargetForest::Layer *TargetForest::Layer::replaceChildWith(ref<Target> const child, TargetForest::Layer *other) const {
  auto result = removeChild(child);
  result->unionWith(other);
  return result;
}

bool TargetForest::Layer::allNodesRefCountOne() const {
  bool all = true;
  for (const auto &it : forest) {
    all &= it.second->_refCount.getCount() == 1;
    assert(all);
    all &= it.second->allNodesRefCountOne();
  }
  return all;
}

void TargetForest::Layer::dump(unsigned n) const {
  llvm::errs() << "THE " << n << " LAYER:\n";
  llvm::errs() << "Confidence: " << confidence << "\n";
  for (const auto &kv : forest) {
    llvm::errs() << kv.first->toString() << "\n";
  }
  llvm::errs() << "-----------------------\n";
  if (!forest.empty()) {
    for (const auto &kv : forest) {
      kv.second->dump(n + 1);
    }
    llvm::errs() << "++++++++++++++++++++++\n";
  }
}

TargetForest::History::EquivTargetsHistoryHashSet
    TargetForest::History::cachedHistories;
TargetForest::History::TargetsHistoryHashSet TargetForest::History::histories;

ref<TargetForest::History> TargetForest::History::create(ref<Target> _target, ref<History> _visitedTargets) {
  History *history = new History(_target, _visitedTargets);

  std::pair<EquivTargetsHistoryHashSet::const_iterator, bool> success =
      cachedHistories.insert(history);
  if (success.second) {
    // Cache miss
    histories.insert(history);
    return history;
  }
  // Cache hit
  delete history;
  history = *(success.first);
  return history;
}

ref<TargetForest::History> TargetForest::History::create(ref<Target> _target) {
  return create(_target, nullptr);
}

ref<TargetForest::History> TargetForest::History::create() {
  return create(nullptr);
}

int TargetForest::History::compare(const History &h) const {
  if (this == &h)
    return 0;

  if (target && h.target) {
    if (target != h.target)
      return (target < h.target) ? -1 : 1;
  } else {
    return h.target ? -1 : (target ? 1 : 0);
  }

  if (visitedTargets && h.visitedTargets) {
    if (h.visitedTargets != h.visitedTargets)
      return (visitedTargets < h.visitedTargets) ? -1 : 1;
  } else {
    return h.visitedTargets ? -1 : (visitedTargets ? 1 : 0);
  }

  return 0;
}

bool TargetForest::History::equals(const History &h) const {
  return compare(h) == 0;
}

void TargetForest::History::dump() const {
  if (target) {
    llvm::errs() << target->toString() << "\n";
  }
  else {
    llvm::errs() << "end.\n";
    assert(!visitedTargets);
    return;
  }
  if (visitedTargets)
    visitedTargets->dump();

}

TargetForest::History::~History() {
  if (histories.find(this) != histories.end()) {
    histories.erase(this);
    cachedHistories.erase(this);
  }
}

void TargetForest::stepTo(ref<Target> loc) {
  if (forest->empty())
    return;
  auto res = forest->find(loc);
  if (res == forest->end()) {
    return;
  }
  if (loc->shouldFailOnThisTarget()) {
    forest = new Layer();
  } else {
    history = history->add(loc);
    forest = forest->replaceChildWith(loc, res->second.get());
    loc->isReported = true;
  }
  if (forest->empty() && !loc->shouldFailOnThisTarget()) {
    history = History::create();
  }
}

void TargetForest::add(ref<Target> target) {
  auto res = forest->find(target);
  if (res != forest->end()) {
    return;
  }
  forest = forest->addChild(target);;
}

void TargetForest::remove(ref<Target> target) {
  auto res = forest->find(target);
  if (res == forest->end()) {
    return;
  }
  forest = forest->removeChild(target);
}

void TargetForest::blockIn(ref<Target> subtarget, ref<Target> target) {
  if (!forest->deepFindIn(subtarget, target)) {
    return;
  }
  forest = forest->blockLeafInChild(subtarget, target);
}

void TargetForest::dump() const {
  llvm::errs() << "History:\n";
  history->dump();
  llvm::errs() << "Forest:\n";
  forest->dump(1);
}

void TargetForest::debugStepToRandomLoc() {
  forest = forest->begin()->second;
}

bool TargetForest::allNodesRefCountOne() const {
  return forest->allNodesRefCountOne();
}

void TargetForest::Layer::addLeafs(
  std::vector<std::pair<ref<Target>, confidence::ty> > *leafs,
  confidence::ty parentConfidence) const {
  for (const auto &targetAndForest : forest) {
    auto target = targetAndForest.first;
    auto layer = targetAndForest.second;
    auto confidence = layer->getConfidence(parentConfidence);
    if (layer->empty()) {
      leafs->push_back(std::make_pair(target, confidence));
    } else {
      layer->addLeafs(leafs, confidence);
    }
  }
}

std::vector<std::pair<ref<Target>, confidence::ty> > *TargetForest::leafs() const {
  auto leafs = new std::vector<std::pair<ref<Target>, confidence::ty> >();
  forest->addLeafs(leafs, forest->getConfidence());
  return leafs;
}

void TargetForest::subtract_confidences_from(TargetForest &other) {
  if (other.empty())
    return;
  forest->subtract_confidences_from(other.forest, forest->getConfidence());
}

void TargetForest::Layer::subtract_confidences_from(ref<Layer> other, confidence::ty parentConfidence) {
  for (auto &targetAndForest : forest) {
    auto target = targetAndForest.first;
    auto &layer = targetAndForest.second;
    auto other_layer_it = other->find(target);
    if (other_layer_it == other->end())
      layer->subtract_confidences_from(other, layer->getConfidence(parentConfidence));
    else {
      layer->confidence = layer->getConfidence(parentConfidence) - other_layer_it->second->confidence;
      if (confidence::isNormal(layer->confidence)) {
        layer->confidence = confidence::MinConfidence; // TODO: we have some bug which we do not want the user to see
      }
    }
  }
}

ref<TargetForest> TargetForest::deep_copy() {
  return new TargetForest(forest->deep_copy(), entryFunction);
}

ref<TargetForest::Layer> TargetForest::Layer::deep_copy() {
  auto copy_forest = new TargetForest::Layer(this);
  for (auto &targetAndForest : forest) {
    auto target = targetAndForest.first;
    auto layer = targetAndForest.second;
    copy_forest->forest[target] = layer->deep_copy();
  }
  return copy_forest;
}

void TargetForest::Layer::divideConfidenceBy(unsigned factor) {
  for (auto &targetAndForest : forest) {
    auto layer = targetAndForest.second;
    layer->confidence /= factor;
  }
}

TargetForest::Layer *TargetForest::Layer::divideConfidenceBy(std::multiset<ref<Target> > &reachableStatesOfTarget) {
  if (forest.empty() || reachableStatesOfTarget.empty())
    return this;
  auto result = new Layer(this);
  for (auto &targetAndForest : forest) {
    auto target = targetAndForest.first;
    auto layer = targetAndForest.second;
    auto count = reachableStatesOfTarget.count(target);
    if (count) {
      if (count == 1)
        continue;
      auto next = new Layer(layer);
      result->forest[target] = next;
      next->confidence /= count;
    } else
      result->forest[target] = layer->divideConfidenceBy(reachableStatesOfTarget);
  }
  return result;
}

void TargetForest::Layer::collectHowManyEventsInTracesWereReached(
  std::unordered_map<unsigned, std::pair<unsigned, unsigned>> &trace2eventCount,
  unsigned reached,
  unsigned total) const {
  total++;
  for (const auto &p : forest) {
    auto target = p.first;
    auto child = p.second;
    auto reachedCurrent = target->isReported ? reached + 1 : reached;
    if (target->shouldFailOnThisTarget()) {
      trace2eventCount[target->getId()] = std::make_pair(reachedCurrent, total);
    }
    child->collectHowManyEventsInTracesWereReached(trace2eventCount, reachedCurrent, total);
  }
}
