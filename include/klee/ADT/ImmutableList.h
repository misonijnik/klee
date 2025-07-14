//===---- ImmutableList.h ---------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_IMMUTABLELIST_H
#define KLEE_IMMUTABLELIST_H

#include <cassert>
#include <memory>
#include <vector>

namespace klee {

template <typename T> class ImmutableList {
  struct ImmutableListNode {
    std::shared_ptr<ImmutableListNode> prev;
    const size_t prev_len;
    std::vector<T> values;

    [[nodiscard]] size_t size() const { return prev_len + values.size(); }

    ImmutableListNode() : prev(nullptr), prev_len(0), values() {}

    explicit ImmutableListNode(const ImmutableList<T> &il)
        : prev_len(il.size()), values() {
      std::shared_ptr<ImmutableListNode> pr = il.node;
      while (pr && pr->values.empty()) {
        pr = pr->prev;
      }
      if (pr && pr->size()) {
        prev = pr;
      } else {
        prev = nullptr;
      }
    }

    ImmutableListNode(std::shared_ptr<ImmutableListNode> prev, size_t prev_len,
                      std::vector<T> values)
        : prev(prev), prev_len(prev_len), values(std::move(values)) {}
  };

  std::shared_ptr<ImmutableListNode> node;

public:
  [[nodiscard]] size_t size() const { return node ? node->size() : 0; }

  struct iterator {
    const ImmutableListNode *rootNode;
    size_t get;

  public:
    explicit iterator(const ImmutableListNode *p) : rootNode(p), get(0) {}

    bool operator==(const iterator &b) const {
      return rootNode == b.rootNode && get == b.get;
    }

    bool operator!=(const iterator &b) const { return !(*this == b); }

    iterator &operator++() {
      ++get;
      return *this;
    }

    const T &operator*() const {
      assert(get < rootNode->size() && "Out of bound");
      const ImmutableListNode *curNode = rootNode;
      while (get < curNode->prev_len) {
        curNode = curNode->prev.get();
      }
      return curNode->values[get - curNode->prev_len];
    }

    const T &operator->() const { return **this; }
  };

  [[nodiscard]] iterator begin() const { return iterator(node.get()); }

  [[nodiscard]] iterator end() const {
    auto it = iterator(node.get());
    it.get = size();
    return it;
  }

  void push_back(T &&value) {
    if (!node) {
      node = std::make_shared<ImmutableListNode>();
    }
    node->values.push_back(std::move(value));
  }

  void push_back(const T &value) {
    if (!node) {
      node = std::make_shared<ImmutableListNode>();
    }
    node->values.push_back(value);
  }

  void pop_back() {
    while (node && node->values.empty()) {
      node = node->prev;
    }
    if (!node) {
      return;
    }
    node = std::make_shared<ImmutableListNode>(node->prev, node->prev_len,
                                               node->values);
    node->values.pop_back();
  }

  T at(size_t index) const {
    auto cur = node;
    while (cur && cur->prev_len > index) {
      cur = cur->prev;
    }
    return cur->values[index - cur->prev_len];
  }

  bool empty() const { return size() == 0; }

  const T &back() const {
    assert(node && "requiers not empty list");
    auto it = iterator(node.get());
    it.get = size() - 1;
    return *it;
  }

  const T &front() const { return at(0); }

  friend bool operator==(const ImmutableList<T> &lhs,
                         const ImmutableList<T> &rhs) {
    if (lhs.size() != rhs.size()) {
      return false;
    }

    auto li = lhs.begin();
    auto ri = rhs.begin();
    while (li != lhs.end() && ri != rhs.end()) {
      if (*li != *ri) {
        return false;
      }
      ++li;
      ++ri;
    }

    return true;
  }

  friend bool operator!=(const ImmutableList<T> &lhs,
                         const ImmutableList<T> &rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const ImmutableList<T> &lhs,
                        const ImmutableList<T> &rhs) {
    if (lhs.size() < rhs.size()) {
      return true;
    } else if (lhs.size() > rhs.size()) {
      return false;
    }

    auto li = lhs.begin();
    auto ri = rhs.begin();
    while (li != lhs.end() && ri != rhs.end()) {
      if (*li < *ri) {
        return true;
      } else if (*ri < *li) {
        return false;
      }
      ++li;
      ++ri;
    }

    return false;
  }

  ImmutableList() : node(){};
  ImmutableList(const ImmutableList<T> &il)
      : node(std::make_shared<ImmutableListNode>(il)) {}
  ImmutableList &operator=(const ImmutableList<T> &il) {
    node = std::make_shared<ImmutableListNode>(il);
    return *this;
  }
};

} // namespace klee

#endif /* KLEE_IMMUTABLELIST_H */
