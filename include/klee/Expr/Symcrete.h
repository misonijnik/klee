#ifndef KLEE_SYMCRETE_H
#define KLEE_SYMCRETE_H

#include "klee/ADT/Ref.h"
#include "klee/Expr/ExprUtil.h"

#include "llvm/Support/Casting.h"

#include <cstdint>
#include <vector>

namespace klee {

class Array;
class Expr;

typedef uint64_t IDType;

class Symcrete {
  friend class ConstraintSet;

public:
  enum class SymcreteKind { SK_COMMON, SK_SIZE };

private:
  static IDType idCounter;
  const SymcreteKind symcreteKind;

  std::vector<ref<Symcrete>> _dependentSymcretes;
  std::vector<const Array *> _dependentArrays;

public:
  const IDType id;
  ReferenceCounter _refCount;

  const ref<Expr> symcretized;

  Symcrete(ref<Expr> e, SymcreteKind kind)
      : symcreteKind(kind), id(idCounter++), symcretized(e) {
    findObjects(e, _dependentArrays);
  }

  Symcrete(ref<Expr> e) : Symcrete(e, SymcreteKind::SK_COMMON) {}
  virtual ~Symcrete() = default;

  void addDependentSymcrete(ref<Symcrete> dependent) {
    _dependentSymcretes.push_back(dependent);
  }

  const std::vector<ref<Symcrete>> &dependentSymcretes() const {
    return _dependentSymcretes;
  }

  const std::vector<const Array *> &dependentArrays() const {
    return _dependentArrays;
  }

  SymcreteKind getKind() const { return symcreteKind; }

  bool equals(const Symcrete &rhs) const { return id == rhs.id; }

  int compare(const Symcrete &rhs) const {
    if (equals(rhs)) {
      return 0;
    }
    return id < rhs.id ? -1 : 1;
  }

  bool operator<(const Symcrete &rhs) const { return compare(rhs) < 0; };
};

class SizeSymcrete : public Symcrete {
public:
  const ref<Symcrete> addressSymcrete;

  SizeSymcrete(ref<Expr> s, ref<Symcrete> address)
      : Symcrete(s, SymcreteKind::SK_SIZE), addressSymcrete(address) {
    addDependentSymcrete(addressSymcrete);
  }

  static bool classof(const Symcrete *symcrete) {
    return symcrete->getKind() == SymcreteKind::SK_SIZE;
  }
};

} // namespace klee

#endif
