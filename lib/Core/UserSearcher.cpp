//===-- UserSearcher.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UserSearcher.h"

#include "Executor.h"
#include "Searcher.h"

#include "klee/Core/Interpreter.h"
#include "klee/Support/ErrorHandling.h"
#include "klee/Support/OptionCategories.h"

#include "klee/Support/CompilerWarning.h"
DISABLE_WARNING_PUSH
DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include "llvm/Support/CommandLine.h"
DISABLE_WARNING_POP

using namespace llvm;
using namespace klee;

namespace {
llvm::cl::OptionCategory
    SearchCat("Search options", "These options control the search heuristic.");

cl::list<Searcher::CoreSearchType> CoreSearch(
    "search",
    cl::desc("Specify the search heuristic (default=random-path interleaved "
             "with nurs:covnew)"),
    cl::values(
        clEnumValN(Searcher::DFS, "dfs", "use Depth First Search (DFS)"),
        clEnumValN(Searcher::BFS, "bfs",
                   "use Breadth First Search (BFS), where scheduling decisions "
                   "are taken at the level of (2-way) forks"),
        clEnumValN(Searcher::RandomState, "random-state",
                   "randomly select a state to explore"),
        clEnumValN(Searcher::RandomPath, "random-path",
                   "use Random Path Selection (see OSDI'08 paper)"),
        clEnumValN(Searcher::NURS_CovNew, "nurs:covnew",
                   "use Non Uniform Random Search (NURS) with Coverage-New"),
        clEnumValN(Searcher::NURS_MD2U, "nurs:md2u",
                   "use NURS with Min-Dist-to-Uncovered"),
        clEnumValN(Searcher::NURS_Depth, "nurs:depth", "use NURS with depth"),
        clEnumValN(Searcher::NURS_RP, "nurs:rp", "use NURS with 1/2^depth"),
        clEnumValN(Searcher::NURS_ICnt, "nurs:icnt",
                   "use NURS with Instr-Count"),
        clEnumValN(Searcher::NURS_CPICnt, "nurs:cpicnt",
                   "use NURS with CallPath-Instr-Count"),
        clEnumValN(Searcher::NURS_QC, "nurs:qc", "use NURS with Query-Cost")),
    cl::cat(SearchCat));

cl::opt<HaltExecution::Reason> UseIterativeDeepeningSearch(
    "use-iterative-deepening-search",
    cl::desc("Use iterative deepening search based on metric (experimental) "
             "(default=unspecified)"),
    cl::values(clEnumValN(HaltExecution::Reason::Unspecified, "unspecified",
                          "Do not use iterative deepening search (default)"),
               clEnumValN(HaltExecution::Reason::MaxTime, "max-time",
                          "metric is maximum time"),
               clEnumValN(HaltExecution::Reason::MaxCycles, "max-cycles",
                          "metric is maximum cycles")),
    cl::init(HaltExecution::Reason::Unspecified), cl::cat(SearchCat));

cl::opt<bool> UseBatchingSearch(
    "use-batching-search",
    cl::desc("Use batching searcher (keep running selected state for N "
             "instructions/time, see --batch-instructions and --batch-time) "
             "(default=false)"),
    cl::init(false), cl::cat(SearchCat));

cl::opt<unsigned> BatchInstructions(
    "batch-instructions",
    cl::desc("Number of instructions to batch when using "
             "--use-batching-search.  Set to 0 to disable (default=10000)"),
    cl::init(10000), cl::cat(SearchCat));

cl::opt<std::string> BatchTime(
    "batch-time",
    cl::desc("Amount of time to batch when using "
             "--use-batching-search.  Set to 0s to disable (default=5s)"),
    cl::init("5s"), cl::cat(SearchCat));

} // namespace

void klee::initializeSearchOptions() {
  // default values
  if (CoreSearch.empty()) {
    CoreSearch.push_back(Searcher::RandomPath);
    CoreSearch.push_back(Searcher::NURS_CovNew);
  }
}

bool klee::userSearcherRequiresMD2U() {
  return (std::find(CoreSearch.begin(), CoreSearch.end(),
                    Searcher::NURS_MD2U) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(),
                    Searcher::NURS_CovNew) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(),
                    Searcher::NURS_ICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(),
                    Searcher::NURS_CPICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_QC) !=
              CoreSearch.end());
}

Searcher *getNewSearcher(Searcher::CoreSearchType type, RNG &rng,
                         PForest &processForest) {
  Searcher *searcher = nullptr;
  switch (type) {
  case Searcher::DFS:
    searcher = new DFSSearcher();
    break;
  case Searcher::BFS:
    searcher = new BFSSearcher();
    break;
  case Searcher::RandomState:
    searcher = new RandomSearcher(rng);
    break;
  case Searcher::RandomPath:
    searcher = new RandomPathSearcher(processForest, rng);
    break;
  case Searcher::NURS_CovNew:
    searcher =
        new WeightedRandomSearcher(WeightedRandomSearcher::CoveringNew, rng);
    break;
  case Searcher::NURS_MD2U:
    searcher = new WeightedRandomSearcher(
        WeightedRandomSearcher::MinDistToUncovered, rng);
    break;
  case Searcher::NURS_Depth:
    searcher = new WeightedRandomSearcher(WeightedRandomSearcher::Depth, rng);
    break;
  case Searcher::NURS_RP:
    searcher = new WeightedRandomSearcher(WeightedRandomSearcher::RP, rng);
    break;
  case Searcher::NURS_ICnt:
    searcher =
        new WeightedRandomSearcher(WeightedRandomSearcher::InstCount, rng);
    break;
  case Searcher::NURS_CPICnt:
    searcher =
        new WeightedRandomSearcher(WeightedRandomSearcher::CPInstCount, rng);
    break;
  case Searcher::NURS_QC:
    searcher =
        new WeightedRandomSearcher(WeightedRandomSearcher::QueryCost, rng);
    break;
  }

  return searcher;
}

Searcher *klee::constructUserSearcher(Executor &executor,
                                      bool stopAfterReachingTarget) {

  Searcher *searcher =
      getNewSearcher(CoreSearch[0], executor.theRNG, *executor.processForest);

  if (CoreSearch.size() > 1) {
    std::vector<Searcher *> s;
    s.push_back(searcher);

    for (unsigned i = 1; i < CoreSearch.size(); i++)
      s.push_back(getNewSearcher(CoreSearch[i], executor.theRNG,
                                 *executor.processForest));

    searcher = new InterleavedSearcher(s);
  }

  if (UseBatchingSearch) {
    searcher = new BatchingSearcher(searcher, time::Span(BatchTime),
                                    BatchInstructions);
  }

  TargetManagerSubscriber *tms = nullptr;
  if (executor.guidanceKind != Interpreter::GuidanceKind::NoGuidance) {
    searcher = new GuidedSearcher(searcher, *executor.distanceCalculator,
                                  executor.theRNG);
    tms = static_cast<GuidedSearcher *>(searcher);
  }

  if (UseIterativeDeepeningSearch != HaltExecution::Reason::Unspecified) {
    searcher = new IterativeDeepeningSearcher(searcher, tms,
                                              UseIterativeDeepeningSearch);
    tms = static_cast<IterativeDeepeningSearcher *>(searcher);
  }

  if (tms)
    executor.targetManager->subscribe(*tms);

  llvm::raw_ostream &os = executor.getHandler().getInfoStream();

  os << "BEGIN searcher description\n";
  searcher->printName(os);
  os << "END searcher description\n";

  return searcher;
}
