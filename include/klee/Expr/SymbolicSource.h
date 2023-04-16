#ifndef KLEE_SYMBOLICSOURCE_H
#define KLEE_SYMBOLICSOURCE_H

#include "klee/ADT/Ref.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace klee {

class Array;
class Expr;
class ConstantExpr;

class SymbolicSource {
protected:
  unsigned hashValue;

public:
  /// @brief Required by klee::ref-managed objects
  class ReferenceCounter _refCount;
  static const unsigned MAGIC_HASH_CONSTANT = 39;

  enum Kind {
    Constant = 3,
    SymbolicSizeConstant,
    SymbolicSizeConstantAddress,
    MakeSymbolic,
    LazyInitializationContent,
    LazyInitializationAddress,
    LazyInitializationSize,
    Instruction,
    Argument
  };

public:
  virtual ~SymbolicSource() {}
  virtual Kind getKind() const = 0;
  virtual std::string getName() const = 0;

  static bool classof(const SymbolicSource *) { return true; }
  unsigned hash() const { return hashValue; }
  virtual unsigned computeHash() = 0;
  virtual std::string toString() const = 0;
  virtual int internalCompare(const SymbolicSource &b) const = 0;
  int compare(const SymbolicSource &b) const;
  bool equals(const SymbolicSource &b) const;
};

class ConstantSource : public SymbolicSource {
public:
  const std::string name;
  /// constantValues - The constant initial values for this array, or empty for
  /// a symbolic array. If non-empty, this size of this array is equivalent to
  /// the array size.
  const std::vector<ref<ConstantExpr>> constantValues;

  ConstantSource(const std::string &_name,
                 const std::vector<ref<ConstantExpr>> &_constantValues)
      : name(_name), constantValues(_constantValues){};
  Kind getKind() const override { return Kind::Constant; }
  virtual std::string getName() const override { return "constant"; }
  uint64_t size() const { return constantValues.size(); }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::Constant;
  }
  static bool classof(const ConstantSource *) { return true; }
  virtual std::string toString() const override { return name; }
  virtual unsigned computeHash() override;

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const ConstantSource &cb = static_cast<const ConstantSource &>(b);
    if (constantValues != cb.constantValues) {
      return constantValues < cb.constantValues ? -1 : 1;
    }
    return 0;
  }
};

class SymbolicSizeConstantSource : public SymbolicSource {
public:
  const unsigned defaultValue;
  SymbolicSizeConstantSource(unsigned _defaultValue)
      : defaultValue(_defaultValue) {}

  Kind getKind() const override { return Kind::SymbolicSizeConstant; }
  virtual std::string getName() const override {
    return "symbolicSizeConstant";
  }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::SymbolicSizeConstant;
  }
  static bool classof(const SymbolicSizeConstantSource *) { return true; }
  virtual std::string toString() const override {
    return getName() + "<default" + llvm::utostr(defaultValue) + ">";
  }

  virtual unsigned computeHash() override;

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const SymbolicSizeConstantSource &ssb =
        static_cast<const SymbolicSizeConstantSource &>(b);
    if (defaultValue != ssb.defaultValue) {
      return defaultValue < ssb.defaultValue ? -1 : 1;
    }
    return 0;
  }
};

class SymbolicSizeConstantAddressSource : public SymbolicSource {
public:
  const unsigned defaultValue;
  const unsigned version;
  SymbolicSizeConstantAddressSource(unsigned _defaultValue, unsigned _version)
      : defaultValue(_defaultValue), version(_version) {}

  Kind getKind() const override { return Kind::SymbolicSizeConstantAddress; }
  virtual std::string getName() const override {
    return "symbolicSizeConstantAddress";
  }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::SymbolicSizeConstantAddress;
  }
  static bool classof(const SymbolicSizeConstantAddressSource *) {
    return true;
  }
  virtual std::string toString() const override {
    return getName() + "<default" + llvm::utostr(defaultValue) + "version" +
           llvm::utostr(version) + ">";
  }

  virtual unsigned computeHash() override;

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const SymbolicSizeConstantAddressSource &ssb =
        static_cast<const SymbolicSizeConstantAddressSource &>(b);
    if (defaultValue != ssb.defaultValue) {
      return defaultValue < ssb.defaultValue ? -1 : 1;
    }
    if (version != ssb.version) {
      return version < ssb.version ? -1 : 1;
    }
    return 0;
  }
};

class MakeSymbolicSource : public SymbolicSource {
  // private:
  //   const std::string &fullName;
public:
  const std::string name;
  const unsigned version;

  MakeSymbolicSource(const std::string &_name, unsigned _version)
      : name(_name), version(_version) {}
  Kind getKind() const override { return Kind::MakeSymbolic; }
  virtual std::string getName() const override { return "makeSymbolic"; }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::MakeSymbolic;
  }
  static bool classof(const MakeSymbolicSource *) { return true; }

  virtual std::string toString() const override {
    return version == 0 ? name : name + llvm::utostr(version);
  }

  virtual unsigned computeHash() override;

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const MakeSymbolicSource &mb = static_cast<const MakeSymbolicSource &>(b);
    if (version != mb.version) {
      return version < mb.version ? -1 : 1;
    }
    if (name != mb.name) {
      return name < mb.name ? -1 : 1;
    }
    return 0;
  }
};

class LazyInitializationSource : public SymbolicSource {
public:
  const ref<Expr> pointer;
  LazyInitializationSource(ref<Expr> _pointer) : pointer(_pointer) {}

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::LazyInitializationAddress ||
           S->getKind() == Kind::LazyInitializationSize ||
           S->getKind() == Kind::LazyInitializationContent;
  }

  virtual unsigned computeHash() override;
  virtual std::string toString() const override;

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const LazyInitializationSource &lib =
        static_cast<const LazyInitializationSource &>(b);
    if (pointer != lib.pointer) {
      return pointer < lib.pointer ? -1 : 1;
    }
    return 0;
  }
};

class LazyInitializationAddressSource : public LazyInitializationSource {
public:
  LazyInitializationAddressSource(ref<Expr> pointer)
      : LazyInitializationSource(pointer) {}
  Kind getKind() const override { return Kind::LazyInitializationAddress; }
  virtual std::string getName() const override {
    return "lazyInitializationAddress";
  }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::LazyInitializationAddress;
  }
  static bool classof(const LazyInitializationAddressSource *) { return true; }
};

class LazyInitializationSizeSource : public LazyInitializationSource {
public:
  LazyInitializationSizeSource(const ref<Expr> &pointer)
      : LazyInitializationSource(pointer) {}
  Kind getKind() const override { return Kind::LazyInitializationSize; }
  virtual std::string getName() const override {
    return "lazyInitializationSize";
  }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::LazyInitializationSize;
  }
  static bool classof(const LazyInitializationSizeSource *) { return true; }
};

class LazyInitializationContentSource : public LazyInitializationSource {
public:
  LazyInitializationContentSource(ref<Expr> pointer)
      : LazyInitializationSource(pointer) {}
  Kind getKind() const override { return Kind::LazyInitializationContent; }
  virtual std::string getName() const override {
    return "lazyInitializationContent";
  }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::LazyInitializationContent;
  }
  static bool classof(const LazyInitializationContentSource *) { return true; }
};

class ValueSource : public SymbolicSource {
public:
  const int index;
  ValueSource(int _index) : index(_index) {}

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::Instruction || S->getKind() == Kind::Argument;
  }

  virtual const llvm::Value &value() const = 0;

  virtual unsigned computeHash() override {
    unsigned res = (getKind() * SymbolicSource::MAGIC_HASH_CONSTANT) +
                   reinterpret_cast<uint64_t>(&value());

    res = (res * SymbolicSource::MAGIC_HASH_CONSTANT) + index;
    hashValue = res;
    return hashValue;
  }

  virtual int internalCompare(const SymbolicSource &b) const override {
    if (getKind() != b.getKind()) {
      return getKind() < b.getKind() ? -1 : 1;
    }
    const ValueSource &vb = static_cast<const ValueSource &>(b);
    if (index != vb.index) {
      return index < vb.index ? -1 : 1;
    }
    if (&value() != &vb.value()) {
      return &value() < &vb.value() ? -1 : 1;
    }
    return 0;
  }
};

class ArgumentSource : public ValueSource {
public:
  const llvm::Argument &allocSite;
  ArgumentSource(const llvm::Argument &_allocSite, int _index)
      : ValueSource(_index), allocSite(_allocSite) {}

  Kind getKind() const override { return Kind::Argument; }
  virtual std::string getName() const override { return "argument"; }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::Argument;
  }

  static bool classof(const ArgumentSource *S) { return true; }

  const llvm::Value &value() const override { return allocSite; }

  virtual std::string toString() const override;
};

class InstructionSource : public ValueSource {
public:
  const llvm::Instruction &allocSite;
  InstructionSource(const llvm::Instruction &_allocSite, int _index)
      : ValueSource(_index), allocSite(_allocSite) {}

  Kind getKind() const override { return Kind::Instruction; }
  virtual std::string getName() const override { return "instruction"; }

  static bool classof(const SymbolicSource *S) {
    return S->getKind() == Kind::Instruction;
  }

  static bool classof(const InstructionSource *S) { return true; }

  const llvm::Value &value() const override { return allocSite; }

  virtual std::string toString() const override;
};

} // namespace klee

#endif /* KLEE_SYMBOLICSOURCE_H */
