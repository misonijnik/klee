//===-- Constraints.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_CONSTRAINTS_H
#define KLEE_CONSTRAINTS_H

#include "klee/ADT/Ref.h"

#include "klee/Expr/Assignment.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Expr/ExprUtil.h"
#include "klee/Expr/Path.h"
#include "klee/Expr/Symcrete.h"

#include <set>
#include <vector>

namespace klee {

class MemoryObject;
struct KInstruction;

/// Resembles a set of constraints that can be passed around
///
class ConstraintSet {
public:
  using constraints_ty = ExprOrderedSet;
  using symcretes_ty = std::set<ref<Symcrete>>;

  ConstraintSet(constraints_ty cs, symcretes_ty symcretes,
                Assignment concretization);
  ConstraintSet();

  void addConstraint(const ref<Expr> &e, const Assignment &delta);
  void addSymcrete(ref<Symcrete> s, const Assignment &concretization);

  void rewriteConcretization(const Assignment &a);
  ConstraintSet withExpr(ref<Expr> e) const;

  std::vector<const Array *> gatherArrays() const;
  std::vector<const Array *> gatherSymcretizedArrays() const;

  bool operator==(const ConstraintSet &b) const {
    return _constraints == b._constraints && _symcretes == b._symcretes;
  }

  bool operator<(const ConstraintSet &b) const {
    return _constraints < b._constraints || _symcretes < b._symcretes;
  }

  void dump() const;

  const constraints_ty &cs() const;
  const symcretes_ty &symcretes() const;
  const Assignment &concretization() const;

private:
  constraints_ty _constraints;
  symcretes_ty _symcretes;
  Assignment _concretization;
};

class PathConstraints {
public:
  void advancePath(KInstruction *ki);
  void addConstraint(const ref<Expr> &e, const Assignment &delta);
  void addSymcrete(ref<Symcrete> s, const Assignment &concretization);
  void rewriteConcretization(const Assignment &a);

  const ConstraintSet &cs() const;
  const Path &path() const;

  static PathConstraints concat(const PathConstraints &l,
                                const PathConstraints &r);

private:
  Path _path;
  ConstraintSet::constraints_ty original;
  ConstraintSet constraints;
  std::map<ref<Expr>, Path::PathIndex> pathIndexes;
  ExprHashMap<ExprHashSet> simplificationMap;
};

struct Conflict {
  Path path;
  ConstraintSet::constraints_ty core;
  std::map<ref<Expr>, Path::PathIndex> pathIndexes;

  Conflict() = default;
};

struct TargetedConflict {
  friend class ref<TargetedConflict>;

private:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;

public:
  Conflict conflict;
  KBlock *target;

  TargetedConflict(Conflict &_conflict, KBlock *_target)
      : conflict(_conflict), target(_target) {}
};

class Simplificator {
public:
  static ref<Expr>
  simplifyExpr(const ConstraintSet::constraints_ty &constraints,
               const ref<Expr> &expr);

  static ref<Expr> simplifyExpr(const ConstraintSet &constraints,
                                const ref<Expr> &expr);

  static void splitAnds(ref<Expr> e, std::vector<ref<Expr>> &exprs);

  Simplificator(const ConstraintSet &constraints) : constraints(constraints) {}

  ConstraintSet simplify();

  ExprHashMap<ExprOrderedSet> &getSimplificationMap();

private:
  bool simplificationDone = false;
  const ConstraintSet &constraints;
  ExprHashMap<ExprOrderedSet> simplificationMap;
};

} // namespace klee

#endif /* KLEE_CONSTRAINTS_H */
