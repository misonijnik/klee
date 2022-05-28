/*
 * This source file has been created by Yummy Research Team. Copyright (c) 2022
 */

// -*- C++ -*-
#include "klee/ADT/TestCaseUtils.h"
#include <cstring>
#include <cstdlib>

ConcretizedObject createConcretizedObject(char *name, unsigned char *values,
                                          unsigned size, Offset *offsets, unsigned n_offsets,
                                          uint64_t address) {
  ConcretizedObject ret;
  ret.name = new char[strlen(name)+1];
  strcpy(ret.name, name);
  ret.size = size;
  ret.address = address;
  ret.values = new unsigned char[size];
  for(size_t i = 0; i<size; i++) {
    ret.values[i] = values[i];
  }

  ret.offsets = new Offset[n_offsets];
  for(size_t i = 0; i<n_offsets; i++) {
    ret.offsets[i] = offsets[i];
  }
  return ret;
}

ConcretizedObject
createConcretizedObject(const char *name, std::vector<unsigned char> &values, uint64_t address) {
  ConcretizedObject ret;
  ret.name = new char[strlen(name)+1];
  strcpy(ret.name, name);
  ret.size = values.size();
  ret.address = address;
  ret.values = new unsigned char[values.size()];
  for(size_t i = 0; i<values.size(); i++) {
    ret.values[i] = values[i];
  }
  ret.n_offsets = 0;
  ret.offsets = nullptr;
  return ret;
}
