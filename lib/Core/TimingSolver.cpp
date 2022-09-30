//===-- TimingSolver.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TimingSolver.h"

#include "ExecutionState.h"

#include "klee/Config/Version.h"
#include "klee/Statistics/Statistics.h"
#include "klee/Statistics/TimerStatIncrementer.h"
#include "klee/Solver/Solver.h"

#include "CoreStats.h"

using namespace klee;
using namespace llvm;

/***/

bool TimingSolver::evaluate(const PathConstraints &constraints, ref<Expr> expr,
                            Solver::Validity &result,
                            SolverQueryMetaData &metaData,
                            bool produceUnsatCore) {
  // Fast path, to avoid timer and OS overhead.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(expr)) {
    result = CE->isTrue() ? Solver::True : Solver::False;
    return true;
  }

  TimerStatIncrementer timer(stats::solverTime);

  if (simplifyExprs)
    expr = ConstraintManager::simplifyExpr(constraints, expr);

  bool success =
      solver->evaluate(Query(constraints, expr, produceUnsatCore), result);

  metaData.queryCost += timer.delta();

  return success;
}

bool TimingSolver::mustBeTrue(const PathConstraints &constraints, ref<Expr> expr,
                              bool &result, SolverQueryMetaData &metaData,
                              bool produceUnsatCore) {
  // Fast path, to avoid timer and OS overhead.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(expr)) {
    result = CE->isTrue() ? true : false;
    return true;
  }

  TimerStatIncrementer timer(stats::solverTime);

  if (simplifyExprs)
    expr = ConstraintManager::simplifyExpr(constraints, expr);

  bool success =
      solver->mustBeTrue(Query(constraints, expr, produceUnsatCore), result);

  metaData.queryCost += timer.delta();

  return success;
}

bool TimingSolver::mustBeFalse(const PathConstraints &constraints, ref<Expr> expr,
                               bool &result, SolverQueryMetaData &metaData,
                               bool produceUnsatCore) {
  return mustBeTrue(constraints, Expr::createIsZero(expr), result, metaData,
                    produceUnsatCore);
}

bool TimingSolver::mayBeTrue(const PathConstraints &constraints, ref<Expr> expr,
                             bool &result, SolverQueryMetaData &metaData,
                             bool produceUnsatCore) {
  bool res;
  if (!mustBeFalse(constraints, expr, res, metaData, produceUnsatCore))
    return false;
  result = !res;
  return true;
}

bool TimingSolver::mayBeFalse(const PathConstraints &constraints, ref<Expr> expr,
                              bool &result, SolverQueryMetaData &metaData,
                              bool produceUnsatCore) {
  bool res;
  if (!mustBeTrue(constraints, expr, res, metaData, produceUnsatCore))
    return false;
  result = !res;
  return true;
}

bool TimingSolver::getValue(const PathConstraints &constraints, ref<Expr> expr,
                            ref<ConstantExpr> &result,
                            SolverQueryMetaData &metaData) {
  // Fast path, to avoid timer and OS overhead.
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(expr)) {
    result = CE;
    return true;
  }

  TimerStatIncrementer timer(stats::solverTime);

  if (simplifyExprs)
    expr = ConstraintManager::simplifyExpr(constraints, expr);

  bool success = solver->getValue(Query(constraints, expr), result);

  metaData.queryCost += timer.delta();

  return success;
}

bool TimingSolver::getInitialValues(
    const PathConstraints &constraints, const std::vector<const Array *> &objects,
    std::vector<std::vector<unsigned char>> &result,
    SolverQueryMetaData &metaData) {
  if (objects.empty())
    return true;

  TimerStatIncrementer timer(stats::solverTime);

  bool success = solver->getInitialValues(
      Query(constraints, ConstantExpr::alloc(0, Expr::Bool)), objects, result);

  metaData.queryCost += timer.delta();

  return success;
}

std::pair<ref<Expr>, ref<Expr>>
TimingSolver::getRange(const PathConstraints &constraints, ref<Expr> expr,
                       SolverQueryMetaData &metaData) {
  TimerStatIncrementer timer(stats::solverTime);
  auto result = solver->getRange(Query(constraints, expr));
  metaData.queryCost += timer.delta();
  return result;
}

void TimingSolver::popUnsatCore(std::vector<ref<Expr>> &unsatCore) {
  solver->popUnsatCore(unsatCore);
}
