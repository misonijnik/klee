/*
 * This source file has been modified by Yummy Research Team. Copyright (c) 2022
 */

#include "klee/Expr/ArrayCache.h"

namespace klee {

ArrayCache::~ArrayCache() {
  // Free Allocated Array objects
  for (ArrayHashMap::iterator ai = cachedSymbolicArrays.begin(),
                              e = cachedSymbolicArrays.end();
       ai != e; ++ai) {
    delete *ai;
  }
  for (ArrayPtrVec::iterator ai = concreteArrays.begin(),
                             e = concreteArrays.end();
       ai != e; ++ai) {
    delete *ai;
  }
}

const Array *
ArrayCache::CreateArray(const std::string &_name, uint64_t _size,
                        int _index, bool _isExternal, ref<Expr> _liSource,
                        const ref<ConstantExpr> *constantValuesBegin,
                        const ref<ConstantExpr> *constantValuesEnd,
                        Expr::Width _domain, Expr::Width _range) {

  const Array *array = new Array(_name, _size, _index, _isExternal, _liSource, constantValuesBegin,
                                 constantValuesEnd, _domain, _range);
  if (array->isSymbolicArray()) {
    std::pair<ArrayHashMap::const_iterator, bool> success =
        cachedSymbolicArrays.insert(array);
    if (success.second) {
      // Cache miss
      return array;
    }
    // Cache hit
    delete array;
    array = *(success.first);
    assert(array->isSymbolicArray() &&
           "Cached symbolic array is no longer symbolic");
    return array;
  } else {
    // Treat every constant array as distinct so we never cache them
    assert(array->isConstantArray());
    concreteArrays.push_back(array); // For deletion later
    return array;
  }
}

const Array *
ArrayCache::CreateArray(const std::string &_name, uint64_t _size,
                        const ref<ConstantExpr> *constantValuesBegin,
                        const ref<ConstantExpr> *constantValuesEnd,
                        Expr::Width _domain, Expr::Width _range) {
  return CreateArray(_name, _size, 0, false, ref<Expr>(), constantValuesBegin, constantValuesEnd, _domain, _range);
}
}
