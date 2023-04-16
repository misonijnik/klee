//===-- IncompleteSolver.cpp ----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Solver/IncompleteSolver.h"

#include "klee/Expr/Constraints.h"

using namespace klee;
using namespace llvm;

/***/

IncompleteSolver::PartialValidity
IncompleteSolver::negatePartialValidity(PartialValidity pv) {
  switch (pv) {
  default:
    assert(0 && "invalid partial validity");
  case MustBeTrue:
    return MustBeFalse;
  case MustBeFalse:
    return MustBeTrue;
  case MayBeTrue:
    return MayBeFalse;
  case MayBeFalse:
    return MayBeTrue;
  case TrueOrFalse:
    return TrueOrFalse;
  }
}

IncompleteSolver::PartialValidity
IncompleteSolver::computeValidity(const Query &query) {
  PartialValidity trueResult = computeTruth(query);

  if (trueResult == MustBeTrue) {
    return MustBeTrue;
  } else {
    PartialValidity falseResult = computeTruth(query.negateExpr());

    if (falseResult == MustBeTrue) {
      return MustBeFalse;
    } else {
      bool trueCorrect = trueResult != None, falseCorrect = falseResult != None;

      if (trueCorrect && falseCorrect) {
        return TrueOrFalse;
      } else if (trueCorrect) { // ==> trueResult == MayBeFalse
        return MayBeFalse;
      } else if (falseCorrect) { // ==> falseResult == MayBeFalse
        return MayBeTrue;
      } else {
        return None;
      }
    }
  }
}

/***/

StagedSolverImpl::StagedSolverImpl(IncompleteSolver *_primary,
                                   Solver *_secondary)
    : primary(_primary), secondary(_secondary) {}

StagedSolverImpl::~StagedSolverImpl() {
  delete primary;
  delete secondary;
}

bool StagedSolverImpl::computeTruth(const Query &query, bool &isValid) {
  IncompleteSolver::PartialValidity trueResult = primary->computeTruth(query);

  if (trueResult != IncompleteSolver::None) {
    isValid = (trueResult == IncompleteSolver::MustBeTrue);
    return true;
  }

  return secondary->impl->computeTruth(query, isValid);
}

bool StagedSolverImpl::computeValidity(const Query &query,
                                       Solver::Validity &result) {
  bool tmp;

  switch (primary->computeValidity(query)) {
  case IncompleteSolver::MustBeTrue:
    result = Solver::True;
    break;
  case IncompleteSolver::MustBeFalse:
    result = Solver::False;
    break;
  case IncompleteSolver::TrueOrFalse:
    result = Solver::Unknown;
    break;
  case IncompleteSolver::MayBeTrue:
    if (!secondary->impl->computeTruth(query, tmp))
      return false;
    result = tmp ? Solver::True : Solver::Unknown;
    break;
  case IncompleteSolver::MayBeFalse:
    if (!secondary->impl->computeTruth(query.negateExpr(), tmp))
      return false;
    result = tmp ? Solver::False : Solver::Unknown;
    break;
  default:
    if (!secondary->impl->computeValidity(query, result))
      return false;
    break;
  }

  return true;
}

bool StagedSolverImpl::computeValue(const Query &query, ref<Expr> &result) {
  if (primary->computeValue(query, result))
    return true;

  return secondary->impl->computeValue(query, result);
}

bool StagedSolverImpl::computeInitialValues(
    const Query &query, const std::vector<const Array *> &objects,
    std::vector<SparseStorage<unsigned char>> &values, bool &hasSolution) {
  if (primary->computeInitialValues(query, objects, values, hasSolution))
    return true;

  return secondary->impl->computeInitialValues(query, objects, values,
                                               hasSolution);
}

bool StagedSolverImpl::check(const Query &query, ref<SolverResponse> &result) {
  ExprHashSet expressions;
  expressions.insert(query.constraints.cs().begin(),
                     query.constraints.cs().end());
  expressions.insert(query.expr);

  std::vector<const Array *> objects;
  findSymbolicObjects(expressions.begin(), expressions.end(), objects);
  std::vector<SparseStorage<unsigned char>> values;

  bool hasSolution;

  bool primaryResult =
      primary->computeInitialValues(query, objects, values, hasSolution);
  if (primaryResult && hasSolution) {
    result = new InvalidResponse(objects, values);
    return true;
  }

  return secondary->impl->check(query, result);
}

bool StagedSolverImpl::computeValidityCore(const Query &query,
                                           ValidityCore &validityCore,
                                           bool &isValid) {
  return secondary->impl->computeValidityCore(query, validityCore, isValid);
}

SolverImpl::SolverRunStatus StagedSolverImpl::getOperationStatusCode() {
  return secondary->impl->getOperationStatusCode();
}

char *StagedSolverImpl::getConstraintLog(const Query &query) {
  return secondary->impl->getConstraintLog(query);
}

void StagedSolverImpl::setCoreSolverTimeout(time::Span timeout) {
  secondary->impl->setCoreSolverTimeout(timeout);
}
