//===-- Constraints.cpp ---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr/Constraints.h"

#include "klee/Expr/Assignment.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Expr/ExprUtil.h"
#include "klee/Expr/ExprVisitor.h"
#include "klee/Expr/Path.h"
#include "klee/Expr/Symcrete.h"
#include "klee/Module/KModule.h"
#include "klee/Support/OptionCategories.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"

#include <map>

using namespace klee;

namespace {
llvm::cl::opt<bool> RewriteEqualities(
    "rewrite-equalities",
    llvm::cl::desc("Rewrite existing constraints when an equality with a "
                   "constant is added (default=true)"),
    llvm::cl::init(true), llvm::cl::cat(SolvingCat));
} // namespace

class ExprReplaceVisitor : public ExprVisitor {
private:
  ref<Expr> src, dst;

public:
  ExprReplaceVisitor(const ref<Expr> &_src, const ref<Expr> &_dst)
      : src(_src), dst(_dst) {}

  Action visitExpr(const Expr &e) override {
    if (e == *src) {
      return Action::changeTo(dst);
    }
    return Action::doChildren();
  }

  Action visitExprPost(const Expr &e) override {
    if (e == *src) {
      return Action::changeTo(dst);
    }
    return Action::doChildren();
  }
};

class ExprReplaceVisitor2 : public ExprVisitor {
private:
  const std::map<ref<Expr>, ref<Expr>> &replacements;

public:
  explicit ExprReplaceVisitor2(
      const std::map<ref<Expr>, ref<Expr>> &_replacements)
      : ExprVisitor(true), replacements(_replacements) {}

  Action visitExprPost(const Expr &e) override {
    auto it = replacements.find(ref<Expr>(const_cast<Expr *>(&e)));
    if (it != replacements.end()) {
      return Action::changeTo(it->second);
    }
    return Action::doChildren();
  }
};

ConstraintSet::ConstraintSet(constraints_ty cs, symcretes_ty symcretes,
                             Assignment concretization)
    : _constraints(cs), _symcretes(symcretes), _concretization(concretization) {
}

ConstraintSet::ConstraintSet() : _concretization(Assignment(true)) {}

void ConstraintSet::addConstraint(const ref<Expr> &e, const Assignment &delta) {
  _constraints.insert(e);
  for (auto i : delta.bindings) {
    _concretization.bindings[i.first] = i.second;
  }
}

IDType Symcrete::idCounter = 0;

void ConstraintSet::addSymcrete(ref<Symcrete> s,
                                const Assignment &concretization) {
  _symcretes.insert(s);
  for (auto i : s->dependentArrays()) {
    _concretization.bindings[i] = concretization.bindings.at(i);
  }
}

void ConstraintSet::rewriteConcretization(const Assignment &a) {
  for (auto i : a.bindings) {
    if (concretization().bindings.count(i.first)) {
      _concretization.bindings[i.first] = i.second;
    }
  }
}

/**
 * @brief Copies the current constraint set and adds expression e.
 *
 * Ideally, this function should accept variadic arguments pack
 * and unpack them with fold expressions, but this feature availible only
 * from C++17.
 *
 * @return copied and modified constraint set.
 */
ConstraintSet ConstraintSet::withExpr(ref<Expr> e) const {
  ConstraintSet newConstraintSet = *this;
  newConstraintSet.addConstraint(e, {});
  return newConstraintSet;
}

void ConstraintSet::dump() const {
  llvm::errs() << "Constraints [\n";
  for (const auto &constraint : _constraints)
    constraint->dump();

  llvm::errs() << "]\n";
}

const ConstraintSet::constraints_ty &ConstraintSet::cs() const {
  return _constraints;
}

const ConstraintSet::symcretes_ty &ConstraintSet::symcretes() const {
  return _symcretes;
}

const Path &PathConstraints::path() const { return _path; }

const Assignment &ConstraintSet::concretization() const {
  return _concretization;
}

const ConstraintSet &PathConstraints::cs() const { return constraints; }

void PathConstraints::advancePath(KInstruction *ki) { _path.advance(ki); }

void PathConstraints::addConstraint(const ref<Expr> &e,
                                    const Assignment &delta) {
  auto expr = Simplificator::simplifyExpr(constraints, e);
  if (auto ce = dyn_cast<ConstantExpr>(expr)) {
    assert(ce->isTrue() && "Attempt to add invalid constraint");
    return;
  }
  original.insert(expr);
  pathIndexes.insert({expr, _path.getCurrentIndex()});
  constraints.addConstraint(expr, delta);
  auto simplificator = Simplificator(constraints);
  constraints = simplificator.simplify();
  ExprHashMap<ExprHashSet> newMap;
  for (auto i : simplificator.getSimplificationMap()) {
    ExprHashSet newSet;
    for (auto j : i.second) {
      for (auto k : simplificationMap[j]) {
        newSet.insert(k);
      }
    }
    newMap.insert({i.first, newSet});
  }
  simplificationMap = newMap;
}

void PathConstraints::addSymcrete(ref<Symcrete> s,
                                  const Assignment &concretization) {
  constraints.addSymcrete(s, concretization);
}

void PathConstraints::rewriteConcretization(const Assignment &a) {
  constraints.rewriteConcretization(a);
}

PathConstraints PathConstraints::concat(const PathConstraints &l,
                                        const PathConstraints &r) {
  // TODO : How to handle symcretes and concretization?
  PathConstraints path = l;
  path._path = Path::concat(l._path, r._path);
  auto offset = l._path.KBlockSize();
  for (const auto &i : r.original) {
    path.original.insert(i);
    auto index = r.pathIndexes.at(i);
    index.block += offset;
    path.pathIndexes.insert({i, index});
  }
  for (const auto &i : r.constraints.cs()) {
    path.constraints.addConstraint(i, {});
    if (r.simplificationMap.count(i)) {
      path.simplificationMap.insert({i, r.simplificationMap.at(i)});
    }
  }
  // Run the simplificator on the newly constructed set?
  return path;
}

ref<Expr>
Simplificator::simplifyExpr(const ConstraintSet::constraints_ty &constraints,
                            const ref<Expr> &expr) {
  if (isa<ConstantExpr>(expr))
    return expr;

  std::map<ref<Expr>, ref<Expr>> equalities;

  for (auto &constraint : constraints) {
    if (const EqExpr *ee = dyn_cast<EqExpr>(constraint)) {
      if (isa<ConstantExpr>(ee->left)) {
        equalities.insert(std::make_pair(ee->right, ee->left));
      } else {
        equalities.insert(
            std::make_pair(constraint, ConstantExpr::alloc(1, Expr::Bool)));
      }
    } else {
      equalities.insert(
          std::make_pair(constraint, ConstantExpr::alloc(1, Expr::Bool)));
    }
  }

  return ExprReplaceVisitor2(equalities).visit(expr);
}

ref<Expr> Simplificator::simplifyExpr(const ConstraintSet &constraints,
                                      const ref<Expr> &expr) {
  return simplifyExpr(constraints.cs(), expr);
}

ConstraintSet Simplificator::simplify() {
  assert(!simplificationDone);
  using EqualityMap = ExprHashMap<std::pair<ref<Expr>, ref<Expr>>>;
  EqualityMap equalities;
  bool changed = true;
  ExprHashMap<ExprOrderedSet> map;
  ConstraintSet::constraints_ty current;
  ConstraintSet::constraints_ty next = constraints.cs();

  for (auto e : next) {
    if (e->getKind() == Expr::Eq) {
      auto be = cast<BinaryExpr>(e);
      if (isa<ConstantExpr>(be->left)) {
        equalities.insert({e, {be->right, be->left}});
      }
    }
  }

  while (changed) {
    changed = false;
    std::vector<EqualityMap::value_type> newEqualitites;
    for (auto eq : equalities) {
      auto visitor = ExprReplaceVisitor(eq.second.first, eq.second.second);
      current = next;
      next.clear();
      for (auto expr : current) {
        ref<Expr> simplifiedExpr;
        if (expr != eq.first && RewriteEqualities) {
          simplifiedExpr = visitor.visit(expr);
        } else {
          simplifiedExpr = expr;
        }
        if (simplifiedExpr != expr) {
          changed = true;
          ExprOrderedSet mapEntry;
          if (auto ce = dyn_cast<ConstantExpr>(simplifiedExpr)) {
            assert(ce->isTrue() && "Constraint simplified to false");
            continue;
          }
          if (map.count(expr)) {
            mapEntry = map.at(expr);
          }
          mapEntry.insert(eq.first);
          std::vector<ref<Expr>> simplifiedExprs;
          splitAnds(simplifiedExpr, simplifiedExprs);
          for (auto simplifiedExpr : simplifiedExprs) {
            if (simplifiedExpr->getKind() == Expr::Eq) {
              auto be = cast<BinaryExpr>(simplifiedExpr);
              if (isa<ConstantExpr>(be->left)) {
                newEqualitites.push_back(
                    {simplifiedExpr, {be->right, be->left}});
              }
            }
          }
          for (auto simplifiedExpr : simplifiedExprs) {
            for (auto i : mapEntry) {
              map[simplifiedExpr].insert(i);
            }
            next.insert(simplifiedExpr);
          }
        } else {
          next.insert(expr);
        }
      }
    }
    for (auto equality : newEqualitites) {
      equalities.insert(equality);
    }
  }

  for (auto entry : map) {
    if (next.count(entry.first)) {
      simplificationMap.insert(entry);
    }
  }
  simplificationDone = true;
  return ConstraintSet(next, constraints.symcretes(),
                       constraints.concretization());
}

ExprHashMap<ExprOrderedSet> &Simplificator::getSimplificationMap() {
  assert(simplificationDone);
  return simplificationMap;
}

void Simplificator::splitAnds(ref<Expr> e, std::vector<ref<Expr>> &exprs) {
  if (auto ce = dyn_cast<ConstantExpr>(e)) {
    assert(ce->isTrue() && "Expression simplified to false");
    return;
  }
  if (auto andExpr = dyn_cast<AndExpr>(e)) {
    splitAnds(andExpr->getKid(0), exprs);
    splitAnds(andExpr->getKid(1), exprs);
  } else {
    exprs.push_back(e);
  }
}

std::vector<const Array *> ConstraintSet::gatherArrays() const {
  std::vector<const Array *> arrays;
  findObjects(_constraints.begin(), _constraints.end(), arrays);
  return arrays;
}

std::vector<const Array *> ConstraintSet::gatherSymcretizedArrays() const {
  std::unordered_set<const Array *> arrays;
  for (const ref<Symcrete> &symcrete : _symcretes) {
    arrays.insert(symcrete->dependentArrays().begin(),
                  symcrete->dependentArrays().end());
  }
  return std::vector<const Array *>(arrays.begin(), arrays.end());
}
