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

#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Module/KInstruction.h"
#include "klee/Core/Path.h"
#include <memory>
#include <string>

namespace klee {

extern llvm::cl::opt<bool> RewriteEqualities;

/// Resembles a set of constraints that can be passed around
///
class PathConstraints {
  friend class ConstraintManager;

public:
  using block_constraints_ty = std::vector<std::pair<KInstruction*, ref<Expr> > >;
  using path_constraints_ty = std::vector<std::pair<KBlock *, block_constraints_ty> >;

  struct base_iterator {
    base_iterator& operator++() {
      ++instr_ptr;
      step();
      return *this;
    }

    friend bool operator!= (const base_iterator& a, const base_iterator& b) {
      bool notBothAtEnd = a.block_ptr != a.block_ptr_end || b.block_ptr != b.block_ptr_end;
      return
        notBothAtEnd
        && (a.block_ptr != b.block_ptr || a.instr_ptr != b.instr_ptr);
    };
  protected:
    path_constraints_ty::const_iterator block_ptr_end;
    path_constraints_ty::const_iterator block_ptr;
    block_constraints_ty::const_iterator instr_ptr;

    base_iterator(const path_constraints_ty::const_iterator &i, const path_constraints_ty::const_iterator &e)
      : block_ptr_end(e), block_ptr(i) {
        if (i == e) {
          block_constraints_ty v;
          instr_ptr = v.cend();
        } else {
          instr_ptr = block_ptr->second.cbegin();
          step();
        }
      }
  private:
    void step() {
      while (block_ptr != block_ptr_end && instr_ptr == block_ptr->second.cend()) {
        ++block_ptr;
        instr_ptr = block_ptr->second.cbegin();
      }
    }
  };

  struct constraint_iterator final : base_iterator {
    using difference_type   = std::ptrdiff_t;
    using value_type        = ref<Expr>;
    using pointer           = value_type *;
    using reference         = value_type &;
    using iterator_category = std::input_iterator_tag;
    constraint_iterator(const path_constraints_ty::const_iterator &i, const path_constraints_ty::const_iterator &e)
      : base_iterator(i, e) {}

    const value_type &operator*() {
      return instr_ptr->second;
    }
  };

  struct loc_constraint_iterator final : base_iterator {
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::pair<KInstruction *, ref<Expr> >;
    using pointer           = value_type *;
    using reference         = value_type &;
    using iterator_category = std::input_iterator_tag;
    loc_constraint_iterator(const path_constraints_ty::const_iterator &i, const path_constraints_ty::const_iterator &e)
      : base_iterator(i, e) {}

    const value_type &operator*() { return *instr_ptr; }
  };
  friend class loc_constraints_iter;
  struct loc_constraints_iter {
    loc_constraints_iter(const PathConstraints *cs) : cs(cs) {}
    loc_constraint_iterator begin() const {
      return loc_constraint_iterator(cs->constraints.cbegin(), cs->constraints.cend());
    }
    loc_constraint_iterator end() const {
      return loc_constraint_iterator(cs->constraints.cend(), cs->constraints.cend());
    };
  private:
    const PathConstraints *cs;
  };

  loc_constraints_iter loc_constraints() const { return loc_constraints_iter(this); }

  bool empty() const;
  constraint_iterator begin() const {
    return constraint_iterator(constraints.cbegin(), constraints.cend());
  }
  constraint_iterator end() const {
    return constraint_iterator(constraints.cend(), constraints.cend());
  };
  size_t size() const noexcept;

  PathConstraints(KBlock *start) {
    block_constraints_ty v;
    constraints.emplace_back(start, v);
  }

  PathConstraints(std::vector<KBlock *> &path) {
    for (auto b : path) {
      block_constraints_ty v;
      constraints.emplace_back(b, v);
    }
  }
  explicit PathConstraints(const std::vector<ref<klee::Expr>> &cs) {
    std::vector<std::pair<KInstruction *, ref<Expr> > > block;
    for (const auto &c : cs) {
      block.emplace_back(nullptr, c);
    }
    constraints.emplace_back(nullptr, block);
  }
  PathConstraints() = default;

  const std::vector<std::pair<ref<Expr>, KInstruction *> > recover(const std::vector<ref<Expr>> &unsatCore) const {
    std::vector<std::pair<ref<Expr>, KInstruction *> > v;
    for (const auto &e : unsatCore)
      v.emplace_back(e, nullptr); //TODO: how to recover locations?
    return v;
  }

  void insert(const ref<Expr> &e, KInstruction *location) {
    auto p = location == nullptr ? nullptr : location->parent;
    if (constraints.empty()) {
      block_constraints_ty v;
      v.emplace_back(location, e);
      constraints.emplace_back(p, v);
      return;
    }
    if (constraints.back().first == p || nullptr == p || location->inst->getOpcode() == llvm::Instruction::Alloca) {
      // Normal execution will add constraints to the last path element
      // We add allocation constraints to the last path element either
      constraints.back().second.emplace_back(location, e);
    } else {
      assert(false);
    }
  }

  void push_back(const ref<Expr> &e);

  bool operator==(const PathConstraints &b) const {
    return constraints == b.constraints;
  }

  void prepend(KBlock *bb) {
    block_constraints_ty v;
    constraints.emplace(constraints.cbegin(), bb, v);
  }

  void remove_first() {
    constraints.erase(constraints.cbegin());
  }

  void append(KBlock *bb) {
    block_constraints_ty v;
    constraints.emplace_back(bb, v);
  }

  const Path buildPath() const {
    std::vector<KBlock *> path;
    for (const auto &bp : constraints) {
      auto b = bp.first;
      if (b != nullptr)
        path.push_back(b);
    }
    return Path(path);
  }

  KBlock *getInitialBlock() const {
    for (const auto &bp : constraints) {
      auto b = bp.first;
      if (b != nullptr)
        return b;
    }
    return nullptr;
  }

  KBlock *getFinalBlock() const {
    for (auto i = constraints.rbegin(), ie = constraints.rend(); i != ie; ++i) {
      auto b = i->first;
      if (b != nullptr)
        return b;
    }
    return nullptr;
  }

  void setPath(KBlock * p) {
    constraints.clear();
    append(p);
  }

  // Merges other constraint path into ours and returns constraint "to be in suffix of our original constraint path"
  ref<Expr> merge(const PathConstraints &b) {
    ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
    ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
    auto ita = constraints.begin();
    auto itae = constraints.end();
    auto itb = b.getConstraints().cbegin();
    auto itbe = b.getConstraints().cend();
    bool mergeStoppedInTheMiddle = false;
    for (; ita != itae && itb != itbe && (*ita).first == (*itb).first; ++ita, ++itb) {
      auto &aIn = (*ita).second;
      const auto &bIn = (*itb).second;
      auto itaIn = aIn.cbegin();
      auto itaIne = aIn.cend();
      auto itbIn = bIn.cbegin();
      auto itbIne = bIn.cend();
      for (; itaIn != itaIne && itbIn != itbIne && *itaIn == *itbIn; ++itaIn, ++itbIn) {}
      if (!(itaIn != itaIne) && !(itbIn != itbIne))
          continue; // whole block constraints are identical
      mergeStoppedInTheMiddle = true;
      add_block(itbIn, itbIne, inB);
      if (!(itaIn != itaIne))
        break;
      auto deleteFrom = itaIn;
      add_block(itaIn, itaIne, inA);
      assert(deleteFrom != itaIne);
      aIn.erase(deleteFrom, itaIne);
    }
    if (!mergeStoppedInTheMiddle && !(ita != itae || itb != itbe)) // our and other constraint paths are identical
      return inA;
    add_blocks(itb, itbe, inB);
    if (ita != itae) {
      auto deleteFrom = ita;
      add_blocks(ita, itae, inA);
      assert(deleteFrom != itae);
      constraints.erase(deleteFrom, itae);
    }
    block_constraints_ty v;
    v.emplace_back(nullptr, OrExpr::create(inA, inB));
    constraints.emplace_back(nullptr, v);
    return inA;
  }

  const path_constraints_ty & getConstraints() const {
    return constraints;
  }

  std::string getPath() const {
    return buildPath().toString();
  }

private:
  static void add_block(block_constraints_ty::const_iterator &itbIn, block_constraints_ty::const_iterator &itbIne, ref<Expr> &inB) {
    for (; itbIn != itbIne; ++itbIn)
      inB = AndExpr::create(inB, (*itbIn).second);
  }

  static void add_blocks(path_constraints_ty::const_iterator itb, path_constraints_ty::const_iterator itbe, ref<Expr> &inB) {
    for (; itb != itbe; ++itb) {
      const auto &bIn = (*itb).second;
      auto itbIn = bIn.cbegin();
      auto itbIne = bIn.cend();
      add_block(itbIn, itbIne, inB);
    }
  }

  path_constraints_ty constraints;
};

class ExprVisitor;

/// Manages constraints, e.g. optimisation
class ConstraintManager {
public:
  /// Create constraint manager that modifies constraints
  /// \param constraints
  explicit ConstraintManager(PathConstraints &constraints);

  /// Simplify expression expr based on constraints
  /// \param constraints set of constraints used for simplification
  /// \param expr to simplify
  /// \return simplified expression
  static ref<Expr> simplifyExpr(const PathConstraints &constraints,
                                const ref<Expr> &expr);

  /// Add constraint to the referenced constraint set
  /// \param constraint
  void addConstraint(const ref<Expr> &constraint, KInstruction *location, bool *sat = 0);

private:
  /// Rewrite set of constraints using the visitor
  /// \param visitor constraint rewriter
  /// \return true iff any constraint has been changed
  bool rewriteConstraints(ExprVisitor &visitor, bool *sat = 0);

  /// Add constraint to the set of constraints
  void addConstraintInternal(const ref<Expr> &constraint, KInstruction *location, bool *sat = 0);

  PathConstraints &constraints;
};

#ifndef divider
#define divider(n) std::string(n, '-') + "\n"
#endif

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const PathConstraints &constraints) {
  if (!constraints.empty()) {
    os << "\n" << divider(30);
    for(const auto &expr: constraints) {
      os << divider(30);
      os << expr << "\n";
      os << divider(30);
    }
    os << divider(30);
  }
  return os;
}

} // namespace klee

#endif /* KLEE_CONSTRAINTS_H */
