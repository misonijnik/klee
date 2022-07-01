//===-- ArrayManager.h --------------------------------------------*- C++
//-*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_ARRAYMANAGER_H
#define KLEE_ARRAYMANAGER_H

#include "klee/Expr/ArrayExprHash.h" // For klee::ArrayHashFn
#include "klee/Expr/Expr.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace klee {

struct WeakEquivArrayCmpFn {
  bool operator()(const Array *array1, const Array *array2) const {
    if (array1 == NULL || array2 == NULL)
      return false;
    if (!array1->liSource.isNull() && !array2->liSource.isNull())
      return array1->liSource == array2->liSource;
    return (array1->size == array2->size) && (array1->name == array2->name);
  }
};

class ArrayManager {
public:
  ArrayManager(ArrayCache *_arrayCache);
  ~ArrayManager();
  /// Create an Array object.
  //
  /// Symbolic Arrays are cached so that only one instance exists. This
  /// provides a limited form of "alpha-renaming". Constant arrays are not
  /// cached.
  ///
  /// This class retains ownership of Array object so that upon destruction
  /// of this object all allocated Array objects are deleted.
  ///
  /// \param _name The name of the array
  /// \param _size The size of the array in bytes
  /// \param constantValuesBegin A pointer to the beginning of a block of
  //         memory that constains a ``ref<ConstantExpr>`` (i.e. concrete values
  //         for the //array). This should be NULL for symbolic arrays.
  /// for symbolic arrays.
  /// \param constantValuesEnd A pointer +1 pass the end of a block of memory
  ///        that contains a ``ref<ConstantExpr>``. This should be NULL for a
  ///        symbolic array.
  /// \param _domain The size of the domain (i.e. the bitvector used to index
  /// the array)
  /// \param _range The size of range (i.e. the bitvector that is indexed to)
  const Array *CreateArray(const std::string &_name, uint64_t _size,
                           const ref<ConstantExpr> *constantValuesBegin = 0,
                           const ref<ConstantExpr> *constantValuesEnd = 0,
                           Expr::Width _domain = Expr::Int32,
                           Expr::Width _range = Expr::Int8);

  const Array *CreateArray(const std::string &name, uint64_t size,
                           bool isExternal, ref<Expr> liSource = ref<Expr>());

  const Array *CreateArray(const Array *array, int index,
                           ref<Expr> liSource = ref<Expr>());

private:
  typedef std::unordered_map<const Array *,
                             std::unordered_map<int, const Array *>,
                             klee::ArrayHashFn, klee::WeakEquivArrayCmpFn>
      ArrayIndexMap;

public:
  ArrayCache *const arrayCache;

private:
  ArrayIndexMap indexedSymbolicArrays;
};
} // namespace klee

#endif /* KLEE_ARRAYMANAGER_H */
