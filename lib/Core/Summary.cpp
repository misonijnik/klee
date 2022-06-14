#include "Summary.h"
#include "CoreStats.h"
#include "Path.h"
#include "klee/ADT/Ref.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprBuilder.h"
#include "klee/Expr/ExprUtil.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/Module/KModule.h"
#include "klee/Support/ErrorHandling.h"
#include "klee/Support/OptionCategories.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace klee;

namespace {
llvm::cl::opt<bool> DebugSummary(
    "debug-summary",
    llvm::cl::desc(""),
    llvm::cl::init(false),
    llvm::cl::cat(klee::DebugCat));
}

#ifndef divider
#define divider(n) std::string(n, '-') + "\n"
#endif

void Summary::summarize(const Path& path, ProofObligation *pob,
                        const SolverQueryMetaData &metaData,
                        ExprHashMap<ref<Expr>> &rebuildMap) {
  if(!metaData.queryValidityCore) {
    return;
  }
  std::string label;
  llvm::raw_string_ostream label_stream(label);

  label_stream << "Add lemma for pob at: " << pob->location->getIRLocation() << (pob->at_return ? "(at return)" : "") << "\n";

  label_stream << "Pob at "
                 << (pob->at_return
                         ? pob->location->getFirstInstruction()->getSourceLocation()
                         : pob->location->getLastInstruction()->getSourceLocation())
                 << "\n";

  auto core = *metaData.queryValidityCore;
  auto &locationLemmas = location_map[pob->location];
  if (locationLemmas.empty())
    ++stats::summarizedLocationCount;
  Lemma* l = new Lemma(path);
  label_stream << "Constraints are:\n";
  for(auto &constraint : core) {
    if(rebuildMap.count(constraint.first)) {
      auto expr = Expr::createIsZero(rebuildMap.at(constraint.first));
      l->constraints.insert(expr);
      label_stream << expr->toString() << "\n";
    }
  }

  label_stream << "State Path is:\n";
  label_stream << path.toString() << "\n";
  label_stream << "Pob Path is:\n";
  label_stream << pob->path.toString() << "\n";
  label_stream << "\n";

  (*summaryFile) << label_stream.str();

  bool exists = false;
  for(auto lemma : path_map[path]) {
    if(*lemma == *l) {
      exists = true;
      break;
    }
  }

  if (!exists) {
    path_map[path].insert(l);
    location_map[path.getFinalBlock()].insert(l);
    lemmas.insert(l);

    if (DebugSummary) {
      llvm::errs() << label_stream.str();
      llvm::errs() << "Summary for pob at " << pob->location->getIRLocation() << "\n";
      llvm::errs() << "Paths:\n";
      llvm::errs() << l->path.toString() << "\n";

      ExprHashSet summary;
      for (auto &expr : l->constraints) {
        summary.insert(expr);
      }
      llvm::errs() << "Lemma:\n";
      llvm::errs() << "\n" << divider(30);
      for (auto &expr : summary) {
        llvm::errs() << divider(30);
        llvm::errs() << expr << "\n";
        llvm::errs() << divider(30);
      }
      llvm::errs() << divider(30);
      llvm::errs() << "\n";
    }
  } else {
    delete l;
  }
}

void Summary::storeLemma(const Lemma *l) {
  uint64_t id = writeLemma(l);
  for (auto constraint : l->constraints) {
    writeExpr(constraint);
    std::vector<const Array *> arrays;
    findSymbolicObjects(constraint, arrays);
    for (auto array : arrays) {
      writeArray(array);
      writeArrayToExpr(constraint, array);
    }
    for (auto array : arrays) {
      for (auto parent : array->parents) {
        writeParentRelation(array, parent);
      }
    }
    writeExprToLemma(l, constraint);
  }
  lemmaDBMap[l] = id;
}

int64_t Summary::writeArray(const Array *a) {
  if (!arrayDBMap.count(a)) {
    arrayDBMap[a] = db->array_write(a);
  }
  return arrayDBMap[a];
}

int64_t Summary::writeExpr(ref<Expr> e) {
  if (!exprDBMap.count(e)) {
    exprDBMap[e] = db->expr_write(e);
  }
  return exprDBMap[e];
}

int64_t Summary::writeLemma(const Lemma *l) {
  if (!lemmaDBMap.count(l)) {
    lemmaDBMap[l] = db->lemma_write(l->path);
  }
  return lemmaDBMap[l];
}

void Summary::writeParentRelation(const Array *child, const Array *parent) {
  assert(arrayDBMap.count(child) && "No child in DB");
  assert(arrayDBMap.count(parent) && "No parent in DB");
  db->parent_write(arrayDBMap[child], arrayDBMap[parent]);
}

void Summary::writeExprToLemma(const Lemma *l, ref<Expr> e) {
  assert(lemmaDBMap.count(l) && "No lemma in DB");
  assert(exprDBMap.count(e) && "No expr in DB");
  db->constraint_write(exprDBMap[e], lemmaDBMap[l]);
}

void Summary::writeArrayToExpr(ref<Expr> e, const Array *a) {
  assert(exprDBMap.count(e) && "No expr in DB");
  assert(arrayDBMap.count(a) && "No array in DB");
  db->arraymap_write(arrayDBMap[a], exprDBMap[e]);
}

void Summary::storeAllToDB() {
  for (auto lemma : lemmas) {
    if (!lemmaDBMap.count(lemma)) {
      storeLemma(lemma);
    }
  }
}

void Summary::loadAllFromDB() {
  ExprBuilder *builder = createDefaultExprBuilder();
  parser = expr::Parser::Create("DBParser",
                                llvm::MemoryBuffer::getMemBuffer("").get(),
                                builder, arrayCache, false);
  auto arrays = db->arrays_retrieve();
  auto parents = db->parents_retrieve();
  for (const auto &parent : parents) {
    arrayParentMap[parent.first].insert(parent.second);
  }
  for (const auto &array : arrays) {
    makeArray(arrays, array.first);
  }
  auto exprs = db->exprs_retrieve();
  makeExprs(exprs);
  auto db_lemmas = db->lemmas_retrieve();
  for (const auto &lemma : db_lemmas) {
    Lemma *l = new Lemma(parse(lemma.second.path, module));
    for (auto expr_id : lemma.second.exprs) {
      l->constraints.insert(exprReverseDBMap[expr_id]);
    }
    path_map[l->path].insert(l);
    location_map[l->path.getFinalBlock()].insert(l);
    lemmas.insert(l);
    lemmaDBMap[l] = lemma.first;
  }
}

void Summary::makeArray(const std::map<uint64_t, std::string> &arrays,
                        uint64_t id) {
  if (arrayReverseDBMap.count(id)) {
    return;
  }
  for (auto parent_id : arrayParentMap[id]) {
    makeArray(arrays, parent_id);
  }
  auto buf = llvm::MemoryBuffer::getMemBuffer(arrays.at(id));
  parser->ResetLexer(buf.get());
  auto array = parser->ParseSingleArray();
  arrayDBMap[array] = id;
  arrayReverseDBMap[id] = array;
}

void Summary::makeExprs(const std::map<uint64_t, std::string> &exprs) {
  for (auto expr_pair : exprs) {
    auto buf = llvm::MemoryBuffer::getMemBuffer(expr_pair.second);
    parser->ResetLexer(buf.get());
    auto expr = parser->ParseSingleExpr();
    exprDBMap[expr] = expr_pair.first;
    exprReverseDBMap[expr_pair.first] = expr;
  }
}
