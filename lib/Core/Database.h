/* -*- mode: c++; c-basic-offset: 2; -*- */
#include <cstdint>
#include <sqlite3.h>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "Path.h"
#include "klee/Expr/Expr.h"

namespace klee {

struct Lemma;

// Wrapper class for database interaction
class Database {
private:
  std::string db_filename;
  ::sqlite3 *db = nullptr;

  void finalize(const char *sql_create, sqlite3_stmt *st);

public:
  Database(std::string &db_filename);
  ~Database();

  struct DBLemma {
    std::string path;
    std::vector<uint64_t> exprs;
    explicit DBLemma(const unsigned char *_path) {
      path = std::string(reinterpret_cast<const char *>(_path));
    }
    DBLemma(const DBLemma &l) {
      path = l.path;
      exprs = l.exprs;
    }
  };

  int64_t array_write(const Array *a);
  int64_t expr_write(ref<Expr> e);
  int64_t lemma_write(const Path &path);
  void parent_write(int64_t child, int64_t parent);
  void constraint_write(int64_t expr, int64_t summary);
  void arraymap_write(int64_t array, int64_t expr);

  std::string array_retrieve(int64_t id);
  std::string expr_retrieve(int64_t id);
  std::string lemma_retrieve(int64_t id);

  std::map<uint64_t, std::string> arrays_retrieve();
  std::map<uint64_t, std::string> exprs_retrieve();
  std::map<uint64_t, DBLemma> lemmas_retrieve();
  std::set<std::pair<uint64_t, uint64_t>> parents_retrieve();

  void create_schema();
  void drop_schema();
};
} // namespace klee
