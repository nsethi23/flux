#pragma once

#include <array>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <memory>

template <typename T, size_t N>
class PoolAllocator {
public:
    PoolAllocator() : next_(0) {
        storage_ = std::make_unique<std::array<T, N>>();
        free_list_.reserve(N);
    }

    T* allocate() {
        if (!free_list_.empty()) {
            T* ptr = free_list_.back();
            free_list_.pop_back();
            return ptr;
        }
        if (next_ >= N) {
            throw std::runtime_error("Pool exhausted");
        }
        return &(*storage_)[next_++];    
    }

    void deallocate(T* ptr) {
        free_list_.push_back(ptr);
    }

    void reset() {
        next_ = 0;
        free_list_.clear();
    }

private:
    alignas(64) std::unique_ptr<std::array<T, N>> storage_;    
    std::vector<T*> free_list_;
    size_t next_;
};