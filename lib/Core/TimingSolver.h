//===-- TimingSolver.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TIMINGSOLVER_H
#define KLEE_TIMINGSOLVER_H

#include "klee/Expr/Constraints.h"
#include "klee/Expr/Expr.h"
#include "klee/Solver/Solver.h"
#include "klee/System/Time.h"

#include <memory>
#include <vector>

namespace klee {
class PathConstraints;
class Solver;

/// TimingSolver - A simple class which wraps a solver and handles
/// tracking the statistics that we care about.
class TimingSolver {
public:
  std::unique_ptr<Solver> solver;
  bool simplifyExprs;

public:
  /// TimingSolver - Construct a new timing solver.
  ///
  /// \param _simplifyExprs - Whether expressions should be
  /// simplified (via the constraint manager interface) prior to
  /// querying.
  TimingSolver(Solver *_solver, bool _simplifyExprs = true)
      : solver(_solver), simplifyExprs(_simplifyExprs) {}

  void setTimeout(time::Span t) { solver->setCoreSolverTimeout(t); }

  char *getConstraintLog(const Query &query) {
    return solver->getConstraintLog(query);
  }

  bool evaluate(const PathConstraints &, ref<Expr>, Solver::Validity &result,
                SolverQueryMetaData &metaData,
                bool produceUnsatCore = false);

  bool mustBeTrue(const PathConstraints &, ref<Expr>, bool &result,
                  SolverQueryMetaData &metaData,
                  bool produceUnsatCore = false);

  bool mustBeFalse(const PathConstraints &, ref<Expr>, bool &result,
                   SolverQueryMetaData &metaData,
                   bool produceUnsatCore = false);

  bool mayBeTrue(const PathConstraints &, ref<Expr>, bool &result,
                 SolverQueryMetaData &metaData,
                 bool produceUnsatCore = false);

  bool mayBeFalse(const PathConstraints &, ref<Expr>, bool &result,
                  SolverQueryMetaData &metaData,
                  bool produceUnsatCore = false);

  bool getValue(const PathConstraints &, ref<Expr> expr,
                ref<ConstantExpr> &result, SolverQueryMetaData &metaData);

  bool getInitialValues(const PathConstraints &,
                        const std::vector<const Array *> &objects,
                        std::vector<std::vector<unsigned char>> &result,
                        SolverQueryMetaData &metaData);

  std::pair<ref<Expr>, ref<Expr>> getRange(const PathConstraints &,
                                           ref<Expr> query,
                                           SolverQueryMetaData &metaData);

  void popUnsatCore(std::vector<ref<Expr>> &unsatCore);
};
}

#endif /* KLEE_TIMINGSOLVER_H */
