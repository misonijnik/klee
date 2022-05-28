/*
 * This source file has been created by Yummy Research Team. Copyright (c) 2022
 */

#include "klee/Expr/ArrayManager.h"
#include "klee/Expr/ArrayCache.h"

namespace klee {

ArrayManager::ArrayManager(ArrayCache *_arrayCache) : arrayCache(_arrayCache) {}

ArrayManager::~ArrayManager() {
  delete arrayCache;
}

const Array *
ArrayManager::CreateArray(const std::string &_name, uint64_t _size,
                        const ref<ConstantExpr> *constantValuesBegin,
                        const ref<ConstantExpr> *constantValuesEnd,
                        Expr::Width _domain, Expr::Width _range) {
  return arrayCache->CreateArray(_name, _size, constantValuesBegin, constantValuesEnd, _domain, _range);
}

const Array *
ArrayManager::CreateArray(const std::string &name, uint64_t size, bool isExternal, ref<Expr> liSource) {
  return arrayCache->CreateArray(name, size, 0, isExternal, liSource);
}

const Array *
ArrayManager::CreateArray(const Array *array, int index, ref<Expr> liSource) {
  if (indexedSymbolicArrays.find(array) != indexedSymbolicArrays.end() &&
      indexedSymbolicArrays.at(array).find(index) != indexedSymbolicArrays.at(array).end()) {
    return indexedSymbolicArrays.at(array).at(index);
  } else {
    liSource = liSource.isNull() ? array->liSource : liSource;
    indexedSymbolicArrays[array][index] =
      arrayCache->CreateArray(array->name, array->size, index, array->isExternal, liSource, &array->constantValues[0],
                              &array->constantValues[0] + array->constantValues.size(), array->domain, array->range);
    return indexedSymbolicArrays.at(array).at(index);
  }
}
}
