//===-- BitwuzlaBuilder.h --------------------------------------------*- C++
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

#include <bitwuzla/cpp/bitwuzla.h>
// #include <smithril.h>
#include <unordered_map>

namespace smithril {
#include <smithril.h>
}
// mixa117

using namespace bitwuzla;

namespace klee {

class BitwuzlaArrayExprHash : public ArrayExprHash<Term> {
  friend class SmithrilBuilder;

public:
  BitwuzlaArrayExprHash(){};
  virtual ~BitwuzlaArrayExprHash();
  void clear();
  void clearUpdates();
};

struct SmithrilTermHash {
  unsigned operator()(const smithril::SmithrilTerm &e) const {
    return (unsigned long)(e._0);
  }
};

struct SmithrilTermCmp {
  bool operator()(const smithril::SmithrilTerm &a, const smithril::SmithrilTerm &b) const {
    return a._0 == b._0;
  }
};
class SmithrilBuilder {
private:
  void FPCastWidthAssert(int *width_out, char const *msg);
  Term fpToIEEEBV(const Term &);

protected:
  Term bvOne(unsigned width);
  Term bvZero(unsigned width);
  Term bvMinusOne(unsigned width);
  Term bvConst32(unsigned width, uint32_t value);
  Term bvConst64(unsigned width, uint64_t value);
  Term bvZExtConst(unsigned width, uint64_t value);
  Term bvSExtConst(unsigned width, uint64_t value);
  Term bvBoolExtract(Term expr, int bit);
  Term bvExtract(Term expr, unsigned top, unsigned bottom);
  Term eqExpr(Term a, Term b);

  // logical left and right shift (not arithmetic)
  Term bvLeftShift(Term expr, unsigned shift);
  Term bvRightShift(Term expr, unsigned shift);
  Term bvVarLeftShift(Term expr, Term shift);
  Term bvVarRightShift(Term expr, Term shift);
  Term bvVarArithRightShift(Term expr, Term shift);

  Term notExpr(Term expr);
  Term andExpr(Term lhs, Term rhs);
  Term orExpr(Term lhs, Term rhs);
  Term iffExpr(Term lhs, Term rhs);

  Term bvNotExpr(Term expr);
  Term bvAndExpr(Term lhs, Term rhs);
  Term bvOrExpr(Term lhs, Term rhs);
  Term bvXorExpr(Term lhs, Term rhs);
  Term bvSignExtend(Term src, unsigned width);

  // Array operations
  Term writeExpr(Term array, Term index, Term value);
  Term readExpr(Term array, Term index);

  // ITE-expression constructor
  Term iteExpr(Term condition, Term whenTrue, Term whenFalse);

  // Bitvector length
  unsigned getBVLength(Term expr);

  // Bitvector comparison
  Term bvLtExpr(Term lhs, Term rhs);
  Term bvLeExpr(Term lhs, Term rhs);
  Term sbvLtExpr(Term lhs, Term rhs);
  Term sbvLeExpr(Term lhs, Term rhs);

  Term constructAShrByConstant(Term expr, unsigned shift, Term isSigned);

  Term getInitialArray(const Array *os);
  Term getArrayForUpdate(const Array *root, const UpdateNode *un);

  Term constructActual(ref<Expr> e, int *width_out);
  smithril::SmithrilTerm construct(ref<Expr> e, int *width_out);
  Term buildArray(const char *name, unsigned indexWidth, unsigned valueWidth);
  Term buildConstantArray(const char *name, unsigned indexWidth,
                          unsigned valueWidth, unsigned value);

  Sort getBoolSort();
  Sort getBvSort(unsigned width);
  smithril::SmithrilSort getBvSortNew(unsigned width);
  Sort getArraySort(Sort domainSort, Sort rangeSort);

  std::pair<unsigned, unsigned> getFloatSortFromBitWidth(unsigned bitWidth);

  // Float casts
  Term castToFloat(const Term &e);
  Term castToBitVector(const Term &e);

  Term getRoundingModeSort(llvm::APFloat::roundingMode rm);
  Term getx87FP80ExplicitSignificandIntegerBit(const Term &e);

  ExprHashMap<std::pair<Term, unsigned>> constructed;
  BitwuzlaArrayExprHash _arr_hash;
  bool autoClearConstructCache;

public:
  smithril::SmithrilContext ctx;
  std::unordered_map<const Array *, std::vector<Term>>
      constant_array_assertions;
  // These are additional constraints that are generated during the
  // translation to Bitwuzla's constraint language. Clients should assert
  // these.
  std::vector<Term> sideConstraints;

  SmithrilBuilder(bool autoClearConstructCache);
  ~SmithrilBuilder();

  Term getTrue();
  Term getFalse();
  Term buildFreshBoolConst();
  smithril::SmithrilTerm getInitialRead(const Array *os, unsigned index);

  smithril::SmithrilTerm construct(ref<Expr> e) {
    smithril::SmithrilTerm res = construct(std::move(e), nullptr);
    if (autoClearConstructCache)
      clearConstructCache();
    return res;
  }
  void clearConstructCache() { constructed.clear(); }
  void clearSideConstraints() { sideConstraints.clear(); }
};
} // namespace klee

#endif
