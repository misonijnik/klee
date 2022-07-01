#pragma once
#include <cstdint>

namespace klee {

template <class T, std::uint32_t startId = 0>
struct Indexer {
    using IndexType = std::uint32_t;

    const IndexType id;
    static IndexType const & getTotalObjects() noexcept {
        return getFreeId();
    }

    Indexer(Indexer const &) : Indexer() {}
    Indexer & operator = (Indexer const &) = delete;

    Indexer(Indexer &&) = default;
    Indexer & operator = (Indexer &&) = default;

protected:
    Indexer() : id(getFreeId()++) {}
    Indexer(IndexType id) : id(id) {}

private:
    struct GlobalIdHolder {
        IndexType id = startId;
    };
    
    static IndexType & getFreeId() noexcept { 
        static GlobalIdHolder freeId; // will be new at every different T value
        return freeId.id;
    }
};

} // namespace klee
