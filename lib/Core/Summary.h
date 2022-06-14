// -*- C++ -*-
#pragma once

#include "Path.h"
#include "ProofObligation.h"
#include "Database.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/Module/KModule.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <map>
#include <vector>
#include <unordered_map>

namespace klee {

struct Lemma {
  Path path;
  ExprHashSet constraints;

  bool operator==(const Lemma &other) {
    return this->path == other.path && this->constraints == other.constraints;
  }
  Lemma(const Path &path) : path(path) {}
};

class Summary {
private:
  llvm::raw_fd_ostream *summaryFile;

  KModule *module;
  ArrayCache *arrayCache;
  Database *db;

  std::map<const Lemma *, int64_t> lemmaDBMap;
  std::map<const Array *, int64_t> arrayDBMap;
  std::map<int64_t, const Array *> arrayReverseDBMap;
  std::map<ref<Expr>, int64_t> exprDBMap;
  std::map<int64_t, ref<Expr>> exprReverseDBMap;
  std::map<uint64_t, std::set<uint64_t>> arrayParentMap;
  expr::Parser *parser;

public:
  std::map<KBlock *, std::set<Lemma *>> location_map;
  std::map<Path, std::set<Lemma *>> path_map;
  std::set<Lemma *> lemmas;

public:
  explicit Summary(llvm::raw_fd_ostream *_summaryFile, std::string _DBFile)
      : summaryFile(_summaryFile), db(new Database(_DBFile)) {}

  void setModule(KModule *_module) { module = _module; }
  void setArrayCache(ArrayCache *cache) { arrayCache = cache; }

  ~Summary() = default;

  void summarize(const Path &path, ProofObligation *pob,
                 const SolverQueryMetaData &metadata,
                 ExprHashMap<ref<Expr>> &rebuildMap);

  void storeLemma(const Lemma *l);

  int64_t writeArray(const Array *a);
  int64_t writeExpr(ref<Expr> e);
  int64_t writeLemma(const Lemma *l);
  void writeParentRelation(const Array *child, const Array *parent);
  void writeExprToLemma(const Lemma *l, ref<Expr> e);
  void writeArrayToExpr(ref<Expr> e, const Array *a);

  void storeAllToDB();
  void loadAllFromDB();

  void makeArray(const std::map<uint64_t, std::string> &arrays, uint64_t id);
  void makeExprs(const std::map<uint64_t, std::string> &exprs);
};

}; // namespace klee
