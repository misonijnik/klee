#include "Database.h"
#include "klee/Core/Path.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprBuilder.h"
#include "klee/Expr/ExprPPrinter.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <sqlite3.h>
#include <string>

namespace klee {

Database::Database(std::string &db_filename) {
  this->db_filename = db_filename;
  if (sqlite3_open(db_filename.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    exit(1);
  }

  char const *sql_check = "SELECT name FROM sqlite_master WHERE type='table'"
                          "AND name='metadata'";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql_check, -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  auto result = sqlite3_step(st);
  if (sqlite3_finalize(st) != SQLITE_OK) {
    exit(1);
  }
  if (result == SQLITE_ROW) {
    return;
  }
  if (result == SQLITE_DONE) {
    create_schema();
    return;
  }
  exit(1);
}

Database::~Database() { sqlite3_close(db); }

void Database::finalize(const char *sql_create, sqlite3_stmt *st) {
  if (sqlite3_prepare_v2(db, sql_create, -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_DONE) {
    exit(1);
  }
  if (sqlite3_finalize(st) != SQLITE_OK) {
    exit(1);
  }
}

void Database::create_schema() {
  sqlite3_stmt *st = nullptr;
  char const *sql_create = "CREATE TABLE metadata("
                           "id INT PRIMARY KEY NOT NULL);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE lemma (id INTEGER NOT NULL PRIMARY KEY, path TEXT);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE array (id INTEGER NOT NULL PRIMARY KEY, array TEXT);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE expr (id INTEGER NOT NULL PRIMARY KEY, expr TEXT);";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE constr"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "summary_id INTEGER REFERENCES lemma(id) ON DELETE CASCADE,"
               "UNIQUE(expr_id, summary_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE arraymap"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "array_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "UNIQUE(array_id, expr_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE parent"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "child_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "parent_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "UNIQUE(child_id, parent_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE functionhash"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "function TEXT,"
               "hash TEXT,"
               "UNIQUE(function, hash))";
  finalize(sql_create, st);
}

void Database::drop_schema() {
  sqlite3_stmt *st = nullptr;
  char const *sql_drop = "DROP TABLE IF EXISTS summary, array, expr,"
                         "constr, arraymap, parent, functionhash";
  finalize(sql_drop, st);
}

int64_t Database::array_write(const Array *A) {
  std::string str;
  llvm::raw_string_ostream o(str);
  ExprPPrinter::printSingleArray(o, A);
  std::string sql = "INSERT INTO array (array) VALUES ('" + str + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    return -1;
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::expr_write(ref<Expr> e) {
  std::string str;
  llvm::raw_string_ostream o(str);
  ExprPPrinter::printSingleExpr(o, e);
  std::string sql = "INSERT INTO expr (expr) VALUES ('" + str + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    return -1;
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::lemma_write(const Path &path) {
  std::string sql = "INSERT INTO lemma (path) "
                    "VALUES ('" +
                    path.toString() + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::functionhash_write(std::string name, size_t functionhash) {
  std::string sql = "INSERT OR IGNORE INTO functionhash (function, hash) "
                    "VALUES ('" +
                    name + "', '" + std::to_string(functionhash) + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

void Database::parent_write(int64_t child, int64_t parent) {
  std::string sql = "INSERT OR IGNORE INTO parent (child_id, parent_id)"
                    "VALUES (" +
                    std::to_string(child) + ", " + std::to_string(parent) +
                    ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::constraint_write(int64_t expr, int64_t summary) {
  std::string sql = "INSERT OR IGNORE INTO constr (expr_id, summary_id)"
                    "VALUES (" +
                    std::to_string(expr) + ", " + std::to_string(summary) +
                    ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}


void Database::arraymap_write(int64_t array, int64_t expr) {
  std::string sql = "INSERT OR IGNORE INTO arraymap (array_id, expr_id)"
                    "VALUES (" +
                    std::to_string(array) + ", " + std::to_string(expr) + ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

std::string Database::array_retrieve(int64_t id) {
  std::string sql =
      "SELECT arr FROM array WHERE rowid = " + std::to_string(id) + ";";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_ROW)
    exit(1);
  std::string arr_string(
      reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return arr_string;
}

std::string Database::expr_retrieve(int64_t id) {
  std::string sql =
      "SELECT expr FROM expr WHERE rowid = " + std::to_string(id) + ";";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_ROW)
    exit(1);
  std::string expr_string(
      reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return expr_string;
}

std::map<uint64_t, Database::DBLemma> Database::lemmas_retrieve() {
  std::map<uint64_t, Database::DBLemma> lemmas;
  std::string sql = "SELECT id, path FROM lemma";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW:
      lemmas.insert(std::make_pair(sqlite3_column_int(st, 0),
                                   DBLemma(sqlite3_column_text(st, 1))));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  for (auto &lemma : lemmas) {
    sql = "SELECT expr_id FROM constr WHERE summary_id = " +
          std::to_string(lemma.first);
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
      exit(1);
    }
    done = false;
    while (!done) {
      switch (sqlite3_step(st)) {
      case SQLITE_ROW:
        lemma.second.exprs.push_back(sqlite3_column_int(st, 0));
        break;
      case SQLITE_DONE:
        done = true;
        break;
      }
    }
    if (sqlite3_finalize(st) != SQLITE_OK)
      exit(1);
  }
  return lemmas;
}

std::map<std::string, size_t> Database::functionhash_retrieve() {
  std::map<std::string, size_t> hashes;
  sqlite3_stmt *st;
  std::string sql = "SELECT function, hash FROM functionhash";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string name(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
      std::string hash_str(reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      std::istringstream iss(hash_str);
      size_t hash;
      iss >> hash;
      hashes.insert(
          std::make_pair(name, hash));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return hashes;
}

std::set<std::pair<uint64_t, uint64_t>> Database::parents_retrieve() {
  std::set<std::pair<uint64_t, uint64_t>> parents;
  sqlite3_stmt *st;
  std::string sql = "SELECT child_id, parent_id FROM parent";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW:
      parents.insert(
          std::make_pair(sqlite3_column_int(st, 0), sqlite3_column_int(st, 1)));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return parents;
}

std::map<uint64_t, std::string> Database::arrays_retrieve() {
  std::map<uint64_t, std::string> arrays;
  sqlite3_stmt *st;
  std::string sql = "SELECT id, array FROM array";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string array_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      arrays.insert(std::make_pair(sqlite3_column_int(st, 0), array_str));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return arrays;
}

std::map<uint64_t, std::string> Database::exprs_retrieve() {
  std::map<uint64_t, std::string> exprs;
  sqlite3_stmt *st;
  std::string sql = "SELECT id, expr FROM expr";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string expr_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      exprs.insert(std::make_pair(sqlite3_column_int(st, 0), expr_str));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return exprs;
}

void Database::lemma_delete(uint64_t id) {
  std::string sql = "DELETE FROM lemma WHERE id = " + std::to_string(id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::hash_delete(std::string name) {
  std::string sql = "DELETE FROM functionhash WHERE function = '" + name + "'";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::exprs_purge() {
  std::string sql = "DELETE from expr where NOT EXISTS (SELECT expr_id FROM "
                    "constr WHERE expr.id = expr_id)";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::arrays_purge() {
  std::string sql = "DELETE from array where NOT EXISTS (SELECT array_id FROM "
                    "arraymap WHERE array.id = array_id)";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

} // namespace klee
