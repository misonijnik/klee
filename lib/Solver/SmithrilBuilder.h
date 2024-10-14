//===-- SmithrilBuilder.h --------------------------------------------*- C++
//-*-====//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef SMITHRIL_BUILDER_H
#define SMITHRIL_BUILDER_H

#include "klee/Expr/ArrayExprHash.h"
#include "klee/Expr/ExprHashMap.h"

#include "llvm/ADT/APFloat.h"

#include <unordered_map>

namespace smithril {
#include <smithril.h>
}
// mixa117

using namespace smithril;

namespace klee {

class SmithrilArrayExprHash : public ArrayExprHash<SmithrilTerm> {
  friend class SmithrilBuilder;

public:
  SmithrilArrayExprHash(){};
  virtual ~SmithrilArrayExprHash();
  void clear();
  void clearUpdates();
};

struct SmithrilTermHash {
  unsigned operator()(const SmithrilTerm &e) const {
    return (unsigned long)(e._0);
  }
};

struct SmithrilTermCmp {
  bool operator()(const SmithrilTerm &a, const SmithrilTerm &b) const {
    return a._0 == b._0;
  }
};
class SmithrilBuilder {
private:
  void FPCastWidthAssert(int *width_out, char const *msg);
  SmithrilTerm fpToIEEEBV(const SmithrilTerm &);

protected:
  SmithrilTerm bvOne(unsigned width);
  SmithrilTerm bvZero(unsigned width);
  SmithrilTerm bvMinusOne(unsigned width);
  SmithrilTerm bvConst32(unsigned width, uint32_t value);
  SmithrilTerm bvConst64(unsigned width, uint64_t value);
  SmithrilTerm bvZExtConst(unsigned width, uint64_t value);
  SmithrilTerm bvSExtConst(unsigned width, uint64_t value);
  SmithrilTerm bvBoolExtract(SmithrilTerm expr, int bit);
  SmithrilTerm bvExtract(SmithrilTerm expr, unsigned top, unsigned bottom);
  SmithrilTerm eqExpr(SmithrilTerm a, SmithrilTerm b);

  // logical left and right shift (not arithmetic)
  SmithrilTerm bvLeftShift(SmithrilTerm expr, unsigned shift);
  SmithrilTerm bvRightShift(SmithrilTerm expr, unsigned shift);
  SmithrilTerm bvVarLeftShift(SmithrilTerm expr, SmithrilTerm shift);
  SmithrilTerm bvVarRightShift(SmithrilTerm expr, SmithrilTerm shift);
  SmithrilTerm bvVarArithRightShift(SmithrilTerm expr, SmithrilTerm shift);

  SmithrilTerm notExpr(SmithrilTerm expr);
  SmithrilTerm andExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm orExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm iffExpr(SmithrilTerm lhs, SmithrilTerm rhs);

  SmithrilTerm bvNotExpr(SmithrilTerm expr);
  SmithrilTerm bvAndExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm bvOrExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm bvXorExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm bvSignExtend(SmithrilTerm src, unsigned width);

  // Array operations
  SmithrilTerm writeExpr(SmithrilTerm array, SmithrilTerm index, SmithrilTerm value);
  SmithrilTerm readExpr(SmithrilTerm array, SmithrilTerm index);

  // ITE-expression constructor
  SmithrilTerm iteExpr(SmithrilTerm condition, SmithrilTerm whenTrue, SmithrilTerm whenFalse);

  // Bitvector length
  unsigned getBVLength(SmithrilTerm expr);

  // Bitvector comparison
  SmithrilTerm bvLtExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm bvLeExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm sbvLtExpr(SmithrilTerm lhs, SmithrilTerm rhs);
  SmithrilTerm sbvLeExpr(SmithrilTerm lhs, SmithrilTerm rhs);

  SmithrilTerm constructAShrByConstant(SmithrilTerm expr, unsigned shift, SmithrilTerm isSigned);

  SmithrilTerm getInitialArray(const Array *os);
  SmithrilTerm getArrayForUpdate(const Array *root, const UpdateNode *un);

  SmithrilTerm constructActual(ref<Expr> e, int *width_out);
  SmithrilTerm construct(ref<Expr> e, int *width_out);
  SmithrilTerm buildArray(const char *name, unsigned indexWidth, unsigned valueWidth);
  SmithrilTerm buildConstantArray(const char *name, unsigned indexWidth,
                          unsigned valueWidth, unsigned value);

  SmithrilSort getBoolSort();
  SmithrilSort getBvSort(unsigned width);
  SmithrilSort getArraySort(SmithrilSort domainSort, SmithrilSort rangeSort);

  std::pair<unsigned, unsigned> getFloatSortFromBitWidth(unsigned bitWidth);

  // Float casts
  SmithrilTerm castToFloat(const SmithrilTerm &e);
  SmithrilTerm castToBitVector(const SmithrilTerm &e);

  RoundingMode getRoundingModeSort(llvm::APFloat::roundingMode rm);
  SmithrilTerm getx87FP80ExplicitSignificandIntegerBit(const SmithrilTerm &e);

  ExprHashMap<std::pair<SmithrilTerm, unsigned>> constructed;
  SmithrilArrayExprHash _arr_hash;
  bool autoClearConstructCache;

public:
  SmithrilContext ctx;
  std::unordered_map<const Array *, std::vector<SmithrilTerm>>
      constant_array_assertions;
  // These are additional constraints that are generated during the
  // translation to Smithril's constraint language. Clients should assert
  // these.
  std::vector<SmithrilTerm> sideConstraints;

  SmithrilBuilder(bool autoClearConstructCache);
  ~SmithrilBuilder();

  SmithrilTerm getTrue();
  SmithrilTerm getFalse();
  SmithrilTerm buildFreshBoolConst();
  SmithrilTerm getInitialRead(const Array *os, unsigned index);

  SmithrilTerm construct(ref<Expr> e) {
    SmithrilTerm res = construct(std::move(e), nullptr);
    if (autoClearConstructCache)
      clearConstructCache();
    return res;
  }
  void clearConstructCache() { constructed.clear(); }
  void clearSideConstraints() { sideConstraints.clear(); }
};
} // namespace klee

#endif
