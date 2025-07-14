#ifndef KLEE_BIDIRECTIONALSEARCHER_H
#define KLEE_BIDIRECTIONALSEARCHER_H

#include "BackwardSearcher.h"
#include "IsolatedStatesInitializer.h"
#include "ObjectManager.h"
#include "Searcher.h"
#include "SearcherUtil.h"
#include "klee/ADT/Ticker.h"

namespace klee {

class IBidirectionalSearcher : public Subscriber {
public:
  virtual ref<SearcherAction> selectAction() = 0;
  virtual bool empty() = 0;
  virtual ~IBidirectionalSearcher() {}
};

class BidirectionalSearcher : public IBidirectionalSearcher {
  enum class StepKind { Forward, Branch, Backward, Initialize };

public:
  ref<SearcherAction> selectAction() override;
  void update(ref<ObjectManager::Event> e) override;
  bool empty() override;

  // Assumes ownership
  explicit BidirectionalSearcher(
      std::unique_ptr<Searcher> _forward, std::unique_ptr<Searcher> _branch,
      std::unique_ptr<BackwardSearcher> _backward,
      std::unique_ptr<IsolatedStatesInitializer> _initializer);

private:
  Ticker ticker;

  std::unique_ptr<Searcher> forward;
  std::unique_ptr<Searcher> branch;
  std::unique_ptr<BackwardSearcher> backward;
  std::unique_ptr<IsolatedStatesInitializer> initializer;

private:
  StepKind selectStep();
};

class ForwardOnlySearcher : public IBidirectionalSearcher {
public:
  ref<SearcherAction> selectAction() override;
  void update(ref<ObjectManager::Event>) override;
  bool empty() override;
  explicit ForwardOnlySearcher(std::unique_ptr<Searcher> searcher);

private:
  std::unique_ptr<Searcher> searcher;
};

} // namespace klee

#endif
