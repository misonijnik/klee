#ifndef KLEE_SPARSESTORAGE_H
#define KLEE_SPARSESTORAGE_H

#include "klee/ADT/PersistentMap.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace llvm {
class raw_ostream;
};

namespace klee {

enum class Density {
  Sparse,
  Dense,
};

enum class MapKind {
  Regular,
  Persistent,
};

// template<typename ValueType>
// class SparseStorageIterator {
//   using RegularIterator = typename std::unordered_map<size_t, ValueType>::const_iterator;
//   using PersistentIterator = typename PersistentMap<size_t, ValueType>::iterator;
// public:

//   // SparseStorageIterator() : kind(MapKind::Regular), regIt(std::nullopt), perIt(std::nullopt) {}
//   SparseStorageIterator(RegularIterator it) : kind(MapKind::Regular), regIt(it), perIt(std::nullopt) {}
//   SparseStorageIterator(PersistentIterator it) : kind(MapKind::Persistent), regIt(std::nullopt), perIt(it) {}

//   SparseStorageIterator& operator++() {
//     if (kind == MapKind::Regular) {
//       ++(*regIt);
//     } else {
//       ++(*perIt);
//     }
//     return *this;
//   }

//   bool operator==(const SparseStorageIterator<ValueType> &other) const {
//     if (kind != other.kind) {
//       return false;
//     }

//     if (kind == MapKind::Regular) {
//       return (*regIt) == (*other.regIt);
//     } else {
//       return (*perIt) == (*other.perIt);
//     }
//   }

//   bool operator!=(const SparseStorageIterator<ValueType> &other) const {
//     return !(*this == other);
//   }

//   std::pair<size_t, ValueType> operator*() const {
//     if (kind == MapKind::Regular) {
//       return *(*regIt);
//     } else {
//       return (*perIt);
//     }
//   }

// private:
//   MapKind kind;
//   std::optional<RegularIterator> regIt;
//   std::optional<PersistentIterator> perIt;
// };

template <typename ValueType>
class SparseStorage_Map {
public:
  SparseStorage_Map(MapKind kind) : kind(kind) {}
  virtual ~SparseStorage_Map() = default;

  virtual bool contains(size_t key) const = 0;
  virtual void erase(size_t key) = 0;
  virtual void set(size_t key, ValueType value) = 0;
  virtual std::optional<ValueType> find(size_t key) const = 0;
  virtual size_t sizeOfSetRange() const = 0;
  virtual size_t mapSize() const = 0;
  virtual void clear() = 0;

  // virtual SparseStorageIterator<ValueType> begin() const = 0;
  // virtual SparseStorageIterator<ValueType> end() const = 0;

  MapKind getKind() const {
    return kind;
  }

private:
  MapKind kind;
};

template <typename ValueType>
class SparseStorage_RegularMap : public SparseStorage_Map<ValueType> {
public:
  SparseStorage_RegularMap() : SparseStorage_Map<ValueType>(MapKind::Regular) {}
  ~SparseStorage_RegularMap() override = default;

  bool contains(size_t key) const override {
    return map.count(key);
  }

  void erase(size_t key) override {
    map.erase(key);
  }
  void set(size_t key, ValueType value) override {
    map[key] = value;
  }

  std::optional<ValueType> find(size_t key) const override {
    auto it = map.find(key);
    if (it != map.end()) {
      return it->second;
    } else {
      return std::nullopt;
    }
  }

  size_t sizeOfSetRange() const override {
    size_t sizeOfRange = 0;
    for (auto i : map) {
      sizeOfRange = std::max(i.first, sizeOfRange);
    }
    return map.empty() ? 0 : sizeOfRange + 1;
  }

  size_t mapSize() const override {
    return map.size();
  }

  void clear() override {
    map.clear();
  }

  static bool classof(const SparseStorage_Map<ValueType> *map) {
    return map->getKind() == MapKind::Regular;
  }

  const std::unordered_map<size_t, ValueType> &getMap() const {
    return map;
  }

  // SparseStorageIterator<ValueType> begin() const override {
  //   return map.cbegin();
  // }

  // SparseStorageIterator<ValueType> end() const override {
  //   return map.cend();
  // }

private:
  std::unordered_map<size_t, ValueType> map;
};

template <typename ValueType>
class SparseStorage_PersistentMap : public SparseStorage_Map<ValueType> {
public:
  SparseStorage_PersistentMap() : SparseStorage_Map<ValueType>(MapKind::Persistent) {}
  ~SparseStorage_PersistentMap() override = default;

  bool contains(size_t key) const override {
    return map.count(key);
  }

  void erase(size_t key) override {
    map.remove(key);
  }
  void set(size_t key, ValueType value) override {
    map.replace({key, value});
  }

  std::optional<ValueType> find(size_t key) const override {
    auto it = map.find(key);
    if (it != map.end()) {
      return it->second;
    } else {
      return std::nullopt;
    }
  }

  size_t sizeOfSetRange() const override {
    size_t sizeOfRange = 0;
    for (auto i : map) {
      sizeOfRange = std::max(i.first, sizeOfRange);
    }
    return map.empty() ? 0 : sizeOfRange + 1;
  }

  size_t mapSize() const override { return map.size(); }

  void clear() override {
    map.clear();
  }

  static bool classof(const SparseStorage_Map<ValueType> *map) {
    return map->getKind() == MapKind::Persistent;
  }

  const PersistentMap<size_t, ValueType> &getMap() const {
    return map;
  }

  // SparseStorageIterator<ValueType> begin() const override {
  //   return map.begin();
  // }

  // SparseStorageIterator<ValueType> end() const override { return map.end(); }

private:
  PersistentMap<size_t, ValueType> map;
};

template <typename ValueType, typename Eq = std::equal_to<ValueType>>
class SparseStorage {
  friend class ObjectState;
private:
  using RegularMap_t = SparseStorage_RegularMap<ValueType>;
  using PersistentMap_t = SparseStorage_PersistentMap<ValueType>;

private:
  ValueType defaultValue;
  std::unique_ptr<SparseStorage_Map<ValueType>> internalStorage;
  Eq eq;
  bool isConstant = false;

  bool contains(size_t key) const { return internalStorage->contains(key) != 0; }

public:
  SparseStorage(const ValueType &defaultValue = ValueType())
      : defaultValue(defaultValue),
        internalStorage(std::make_unique<RegularMap_t>()) {}

  SparseStorage(const std::unordered_map<size_t, ValueType> &internalStorage,
                const ValueType &defaultValue)
      : defaultValue(defaultValue),
        internalStorage(std::make_unique<RegularMap_t>()) {
    for (auto &[index, value] : internalStorage) {
      store(index, value);
    }
  }

  SparseStorage(const std::vector<ValueType> &values,
                const ValueType &defaultValue = ValueType())
      : defaultValue(defaultValue),
        internalStorage(std::make_unique<RegularMap_t>()) {
    for (size_t idx = 0; idx < values.size(); ++idx) {
      store(idx, values[idx]);
    }
  }

  SparseStorage(const SparseStorage<ValueType, Eq> &other)
      : defaultValue(other.defaultValue), isConstant(other.isConstant) {
    if (auto regularMap =
            llvm::dyn_cast<RegularMap_t>(other.internalStorage.get())) {
      internalStorage = std::make_unique<RegularMap_t>(*regularMap);
    } else if (auto persistentMap = llvm::dyn_cast<PersistentMap_t>(
                   other.internalStorage.get())) {
      internalStorage = std::make_unique<PersistentMap_t>(*persistentMap);
    }
  }

  SparseStorage& operator=(const SparseStorage<ValueType, Eq> &other) {
    if (this != &other) {
      defaultValue = other.defaultValue;
      isConstant = other.isConstant;
      if (auto regularMap = llvm::dyn_cast<RegularMap_t>(other.internalStorage.get())) {
        internalStorage = std::make_unique<RegularMap_t>(*regularMap);
      } else if (auto persistentMap = llvm::dyn_cast<PersistentMap_t>(other.internalStorage.get())) {
        internalStorage = std::make_unique<PersistentMap_t>(*persistentMap);
      }
    }
    return *this;
  }

  void store(size_t idx, const ValueType &value) {
    if (eq(value, defaultValue)) {
      internalStorage->erase(idx);
    } else {
      internalStorage->set(idx, value);
    }

    if (internalStorage->mapSize() > 0 && !isConstant) {
      if (auto regularMap =
              llvm::dyn_cast<RegularMap_t>(internalStorage.get())) {
        PersistentMap_t *newStorage = new PersistentMap_t();
        for (const auto &i : regularMap->getMap()) {
          newStorage->set(i.first, i.second);
        }
        internalStorage = std::unique_ptr<PersistentMap_t>(newStorage);
      }
    }
  }

  template <typename InputIterator>
  void store(size_t idx, InputIterator iteratorBegin,
             InputIterator iteratorEnd) {
    for (; iteratorBegin != iteratorEnd; ++iteratorBegin, ++idx) {
      store(idx, *iteratorBegin);
    }
  }

  ValueType load(size_t idx) const {
    auto it = internalStorage->find(idx);
    return it ? *it : defaultValue;
  }

  size_t sizeOfSetRange() const {
    return internalStorage->sizeOfSetRange();
  }

  bool operator==(const SparseStorage<ValueType> &another) const {
    return eq(defaultValue, another.defaultValue) && compare(another) == 0;
  }

  bool operator!=(const SparseStorage<ValueType> &another) const {
    return !(*this == another);
  }

  bool operator<(const SparseStorage &another) const {
    return compare(another) == -1;
  }

  bool operator>(const SparseStorage &another) const {
    return compare(another) == 1;
  }

  int compare(const SparseStorage<ValueType> &other) const {
    auto ordered = calculateOrderedStorage();
    auto otherOrdered = other.calculateOrderedStorage();

    if (ordered == otherOrdered) {
      return 0;
    } else {
      return ordered < otherOrdered ? -1 : 1;
    }
  }

  std::map<size_t, ValueType> calculateOrderedStorage() const {
    std::map<size_t, ValueType> ordered;
    if (auto storage = llvm::dyn_cast<RegularMap_t>(internalStorage.get())) {
      for (const auto &i : storage->getMap()) {
        ordered.insert(i);
      }
    } else if (auto storage =
                   llvm::dyn_cast<PersistentMap_t>(internalStorage.get())) {
      for (const auto &i : storage->getMap()) {
        ordered.insert(i);
      }
    }
    return ordered;
  }

  std::vector<ValueType> getFirstNIndexes(size_t n) const {
    std::vector<ValueType> vectorized(n);
    for (size_t i = 0; i < n; i++) {
      vectorized[i] = load(i);
    }
    return vectorized;
  }

  const SparseStorage_Map<ValueType> *storage() const {
    return internalStorage.get();
  };

  const ValueType &defaultV() const { return defaultValue; };

  void reset() { internalStorage->clear(); }

  void reset(ValueType newDefault) {
    defaultValue = newDefault;
    reset();
  }

  void print(llvm::raw_ostream &os, Density) const;
};

template <typename U>
SparseStorage<unsigned char> sparseBytesFromValue(const U &value) {
  const unsigned char *valueUnsignedCharIterator =
      reinterpret_cast<const unsigned char *>(&value);
  SparseStorage<unsigned char> result;
  result.store(0, valueUnsignedCharIterator,
               valueUnsignedCharIterator + sizeof(value));
  return result;
}

} // namespace klee

#endif
