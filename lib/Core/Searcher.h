//===-- Searcher.h ----------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_SEARCHER_H
#define KLEE_SEARCHER_H

#include "ExecutionState.h"
#include "PForest.h"
#include "PTree.h"
#include "klee/ADT/RNG.h"
#include "klee/Module/KModule.h"
#include "klee/System/Time.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <queue>
#include <set>
#include <vector>
#include <unordered_map>

namespace llvm {
  class BasicBlock;
  class Function;
  class Instruction;
  class raw_ostream;
}

namespace klee {
  class CodeGraphDistance;
  template<class T, class Comparator> class DiscretePDF;
  template<class T, class Comparator> class WeightedQueue;
  class ExecutionState;
  class Executor;
  class TargetForest;

  /// A Searcher implements an exploration strategy for the Executor by selecting
  /// states for further exploration using different strategies or heuristics.
  class Searcher {
  public:
    virtual ~Searcher() = default;

    /// Selects a state for further exploration.
    /// \return The selected state.
    virtual ExecutionState &selectState() = 0;

    /// Notifies searcher about new or deleted states.
    /// \param current The currently selected state for exploration.
    /// \param addedStates The newly branched states with `current` as common ancestor.
    /// \param removedStates The states that will be terminated.
    virtual void update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) = 0;

    /// \return True if no state left for exploration, False otherwise
    virtual bool empty() = 0;

    /// Prints name of searcher as a `klee_message()`.
    // TODO: could probably made prettier or more flexible
    virtual void printName(llvm::raw_ostream &os) = 0;

    enum CoreSearchType : std::uint8_t {
      DFS,
      BFS,
      RandomState,
      RandomPath,
      NURS_CovNew,
      NURS_MD2U,
      NURS_Depth,
      NURS_RP,
      NURS_ICnt,
      NURS_CPICnt,
      NURS_QC
    };
  };

  /// DFSSearcher implements depth-first exploration. All states are kept in
  /// insertion order. The last state is selected for further exploration.
  class DFSSearcher final : public Searcher {
    std::vector<ExecutionState*> states;

  public:
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// BFSSearcher implements breadth-first exploration. When KLEE branches multiple
  /// times for a single instruction, all new states have the same depth. Keep in
  /// mind that the process tree (PTree) is a binary tree and hence the depth of
  /// a state in that tree and its branch depth during BFS are different.
  class BFSSearcher final : public Searcher {
    std::deque<ExecutionState*> states;

  public:
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// RandomSearcher picks a state randomly.
  class RandomSearcher final : public Searcher {
    std::vector<ExecutionState*> states;
    RNG &theRNG;

  public:
    explicit RandomSearcher(RNG &rng);
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// TargetedSearcher picks a state /*COMMENT*/.
  class TargetedSearcher final : public Searcher {
  public:
    enum WeightResult : std::uint8_t {
      Continue,
      Done,
      Miss,
    };

  private:
    typedef unsigned weight_type;

    std::unique_ptr<WeightedQueue<ExecutionState *, ExecutionStateIDCompare>>
        states;
    ref<Target> target;
    CodeGraphDistance &codeGraphDistance;
    const std::unordered_map<KFunction *, unsigned int> &distanceToTargetFunction;
    std::set<ExecutionState *> reachedOnLastUpdate;

    bool distanceInCallGraph(KFunction *kf, KBlock *kb, unsigned int &distance);
    WeightResult tryGetLocalWeight(ExecutionState *es, weight_type &weight,
                                   const std::vector<KBlock *> &localTargets);
    WeightResult tryGetPreTargetWeight(ExecutionState *es, weight_type &weight);
    WeightResult tryGetTargetWeight(ExecutionState *es, weight_type &weight);
    WeightResult tryGetPostTargetWeight(ExecutionState *es, weight_type &weight);
    WeightResult tryGetWeight(ExecutionState *es, weight_type &weight);

  public:
    TargetedSearcher(ref<Target> target, CodeGraphDistance &distance);
    ~TargetedSearcher() override;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool updateCheckCanReach(
                ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates);
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
    std::set<ExecutionState *> reached();
    void removeReached();
  };

  class GuidedSearcher final : public Searcher {
  private:
    template <class T>
    class TargetHashMap
        : public std::unordered_map<ref<Target>, T, RefTargetHash, RefTargetCmp> {};
    using TargetToSearcherMap = TargetHashMap<std::unique_ptr<TargetedSearcher>>;
    using TargetToStateSetMap = TargetHashMap<std::set<ExecutionState *, ExecutionStateIDCompare>>;
    using TargetToStateVectorMap = TargetHashMap<std::vector<ExecutionState *>>;

    template <class T>
    class TargetForestHistoryHashMap
        : public std::unordered_map<ref<TargetForest::History>, T, RefTargetsHistoryHash, RefTargetsHistoryCmp> {};
    template <class T>
    class TargetForestHistoryTargetsHashMap
        : public TargetForestHistoryHashMap<TargetHashMap<T>> {};

    using TargetForestHistoryToSearcherMap = TargetForestHistoryTargetsHashMap<std::unique_ptr<TargetedSearcher>>;
    using TargetForestHistoryToStateSetMap = TargetForestHistoryTargetsHashMap<std::set<ExecutionState *>>;
    using TargetForestHisoryToStateVectorMap = TargetForestHistoryTargetsHashMap<std::vector<ExecutionState *>>;
    using TargetForestHisoryToTargetSet = TargetForestHistoryHashMap<std::unordered_set<ref<Target>, RefTargetHash, RefTargetCmp>>;
    using TargetForestHisoryToTargetVector = TargetForestHistoryHashMap<std::vector<ref<Target>>>;
    using TargetForestHisoryTargetVector = std::vector<std::pair<ref<TargetForest::History>, ref<Target>>>;

    enum Guidance {
      CoverageGuidance,
      ErrorGuidance
    };

    Guidance guidance;
    std::unique_ptr<Searcher> baseSearcher;
    TargetForestHistoryToSearcherMap targetedSearchers;
    CodeGraphDistance &codeGraphDistance;
    TargetCalculator *stateHistory;
    TargetForestHisoryToTargetSet reachedTargets;
    std::set<ExecutionState *, ExecutionStateIDCompare> &removedButReachableStates;
    std::set<ExecutionState *, ExecutionStateIDCompare> &pausedStates;
    std::size_t bound;
    RNG &theRNG;
    unsigned index{1};

    std::vector<ExecutionState *> baseAddedStates;
    std::vector<ExecutionState *> baseRemovedStates;
    std::vector<ExecutionState *> targetedAddedStates;
    std::vector<ExecutionState *> targetlessStates;
    TargetForestHisoryTargetVector historiesAndTargets;

    bool tryAddTarget(ref<TargetForest::History> history, ref<Target> target);
    TargetForestHisoryTargetVector::iterator
    removeTarget(ref<TargetForest::History> history, ref<Target> target);
    bool isStuck(ExecutionState &state);
    void innerUpdate(ExecutionState *current,
                     const std::vector<ExecutionState *> &addedStates,
                     const std::vector<ExecutionState *> &removedStates);
    bool
    updateTargetedSearcher(ref<TargetForest::History> history,
                           ref<Target> target, ExecutionState *current,
                           std::vector<ExecutionState *> &addedStates,
                           std::vector<ExecutionState *> &removedStates);

    void clearReached(const std::vector<ExecutionState *> &removedStates);
    void collectReached(TargetToStateSetMap &reachedStates);
    bool isReached(ref<TargetForest::History> history, ref<Target> target);

  public:
    GuidedSearcher(
        Searcher *baseSearcher, CodeGraphDistance &codeGraphDistance,
        TargetCalculator &stateHistory,
        std::set<ExecutionState *, ExecutionStateIDCompare> &removedButReachableStates,
        std::set<ExecutionState *, ExecutionStateIDCompare> &pausedStates,
        std::size_t bound, RNG &rng);
    GuidedSearcher(
        CodeGraphDistance &codeGraphDistance,
        std::set<ExecutionState *, ExecutionStateIDCompare> &removedButReachableStates,
        std::set<ExecutionState *, ExecutionStateIDCompare> &pausedStates,
        std::size_t bound, RNG &rng);
    ~GuidedSearcher() override = default;
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    void update(
        ExecutionState *current,
        const std::vector<ExecutionState *> &addedStates,
        const std::vector<ExecutionState *> &removedStates,
        TargetToStateSetMap &reachedStates);

    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// The base class for all weighted searchers. Uses DiscretePDF as underlying
  /// data structure.
  class WeightedRandomSearcher final : public Searcher {
  public:
    enum WeightType : std::uint8_t {
      Depth,
      RP,
      QueryCost,
      InstCount,
      CPInstCount,
      MinDistToUncovered,
      CoveringNew
    };

  private:
    std::unique_ptr<DiscretePDF<ExecutionState*, ExecutionStateIDCompare>> states;
    RNG &theRNG;
    WeightType type;
    bool updateWeights;
    
    double getWeight(ExecutionState*);

  public:
    /// \param type The WeightType that determines the underlying heuristic.
    /// \param RNG A random number generator.
    WeightedRandomSearcher(WeightType type, RNG &rng);
    ~WeightedRandomSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// RandomPathSearcher performs a random walk of the PTree to select a state.
  /// PTree is a global data structure, however, a searcher can sometimes only
  /// select from a subset of all states (depending on the update calls).
  ///
  /// To support this, RandomPathSearcher has a subgraph view of PTree, in that it
  /// only walks the PTreeNodes that it "owns". Ownership is stored in the
  /// getInt method of the PTreeNodePtr class (which hides it in the pointer itself).
  ///
  /// The current implementation of PTreeNodePtr supports only 3 instances of the
  /// RandomPathSearcher. This is because the current PTreeNodePtr implementation
  /// conforms to C++ and only steals the last 3 alignment bits. This restriction
  /// could be relaxed slightly by an architecture-specific implementation of
  /// PTreeNodePtr that also steals the top bits of the pointer.
  ///
  /// The ownership bits are maintained in the update method.
  class RandomPathSearcher final : public Searcher {
    PForest &processForest;
    RNG &theRNG;

    // Unique bitmask of this searcher
    const uint8_t idBitMask;

  public:
    /// \param processTree The process tree.
    /// \param RNG A random number generator.
    RandomPathSearcher(PForest &processForest, RNG &rng);
    ~RandomPathSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };


  extern llvm::cl::opt<bool> UseIncompleteMerge;
  class MergeHandler;
  class MergingSearcher final : public Searcher {
    friend class MergeHandler;

    private:

    std::unique_ptr<Searcher> baseSearcher;

    /// States that have been paused by the 'pauseState' function
    std::vector<ExecutionState*> pausedStates;

    public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    explicit MergingSearcher(Searcher *baseSearcher);
    ~MergingSearcher() override = default;

    /// ExecutionStates currently paused from scheduling because they are
    /// waiting to be merged in a klee_close_merge instruction
    std::set<ExecutionState *> inCloseMerge;

    /// Keeps track of all currently ongoing merges.
    /// An ongoing merge is a set of states (stored in a MergeHandler object)
    /// which branched from a single state which ran into a klee_open_merge(),
    /// and not all states in the set have reached the corresponding
    /// klee_close_merge() yet.
    std::vector<MergeHandler *> mergeGroups;

    /// Remove state from the searcher chain, while keeping it in the executor.
    /// This is used here to 'freeze' a state while it is waiting for other
    /// states in its merge group to reach the same instruction.
    void pauseState(ExecutionState &state);

    /// Continue a paused state
    void continueState(ExecutionState &state);

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;

    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// BatchingSearcher selects a state from an underlying searcher and returns
  /// that state for further exploration for a given time or a given number
  /// of instructions.
  class BatchingSearcher final : public Searcher {
    std::unique_ptr<Searcher> baseSearcher;
    time::Span timeBudget;
    unsigned instructionBudget;

    ExecutionState *lastState {nullptr};
    time::Point lastStartTime;
    unsigned lastStartInstructions;

  public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    /// \param timeBudget Time span a state gets selected before choosing a different one.
    /// \param instructionBudget Number of instructions to re-select a state for.
    BatchingSearcher(Searcher *baseSearcher, time::Span timeBudget, unsigned instructionBudget);
    ~BatchingSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// IterativeDeepeningTimeSearcher implements time-based deepening. States
  /// are selected from an underlying searcher. When a state reaches its time
  /// limit it is paused (removed from underlying searcher). When the underlying
  /// searcher runs out of states, the time budget is increased and all paused
  /// states are revived (added to underlying searcher).
  class IterativeDeepeningTimeSearcher final : public Searcher {
    std::unique_ptr<Searcher> baseSearcher;
    time::Point startTime;
    time::Span time {time::seconds(1)};
    std::set<ExecutionState*> pausedStates;

  public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    explicit IterativeDeepeningTimeSearcher(Searcher *baseSearcher);
    ~IterativeDeepeningTimeSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// InterleavedSearcher selects states from a set of searchers in round-robin
  /// manner. It is used for KLEE's default strategy where it switches between
  /// RandomPathSearcher and WeightedRandomSearcher with CoveringNew metric.
  class InterleavedSearcher final : public Searcher {
    std::vector<std::unique_ptr<Searcher>> searchers;
    unsigned index {1};

  public:
    /// \param searchers The underlying searchers (takes ownership).
    explicit InterleavedSearcher(const std::vector<Searcher *> &searchers);
    ~InterleavedSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

} // klee namespace

#endif /* KLEE_SEARCHER_H */
