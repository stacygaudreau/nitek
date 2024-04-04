/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file mempool.h
 *  @brief Memory pool for low-latency, dynamically allocated objects on the heap
 *  @author Stacy Gaudreau
 *  @date 2024.03.30
 *
 */


#pragma once


#include <cstdint>
#include <vector>
#include <string>
#include "macros.h"


namespace Utils
{
/**
 * @brief A low-latency memory pool for storing dynamically allocated objects on the heap
 * @tparam T Type of object to store in the pool
 * @details The memory pool should be created *before* the execution of any critical paths. This
 * is because the contained vector being resized is the only time when dynamic memory
 * allocation occurs.
 */
template<typename T>
class MemPool final {
public:
    /**
     * @brief Construct a low latency memory pool for n_blocks of type T.
     * @param n_blocks (max) number of blocks the pool can store
     */
    explicit MemPool(std::size_t n_blocks) : blocks(n_blocks, { T{ }, true }) {
        // ensure that the first block in the pool is the correct type; we use reinterpret_cast in
        // .deallocate() - for performance reasons - thus, we ensure cast safety here instead.
        ASSERT(reinterpret_cast<const Block*>(&(blocks[0].object)) == &(blocks[0]),
               "MemPool::Stored object must be first member of Block");
    }
    /**
     * @brief Allocate a new memory block for object of type T
     * @tparam Args Variadic template arguments for T's constructor
     * @param args Zero or more arguments, passed to T's constructor
     * @return New object of type T, or nullptr if unsuccessful
     */
    template<typename ...Args>
    T* allocate(Args... args) noexcept {
        auto block = &(blocks[i_next_free]);
        ASSERT(block->is_free, "MemPool::object block at index " +
                std::to_string(i_next_free) + " is not free");
        T* object = &(block->object);
        object = new(object) T(args...);  // use a specific memory block to allocate via new()
        block->is_free = false;
        update_next_free_index();
        return object;
    }
    /**
     * @brief Deallocate/free a given object's block from the memory pool
     * @param object Object to deallocate
     */
    auto deallocate(const T* object) noexcept {
        const auto i_object = reinterpret_cast<const Block*>(object) - &blocks[0];
        ASSERT(i_object >= 0 && static_cast<size_t>(i_object) < blocks.size(),
               "MemPool::object being deallocated does not belong to this pool");
        ASSERT(!blocks[i_object].is_free,
               "MemPool::attempting to free a pool object which is NOT in use at index "
                       + std::to_string(i_object));
        blocks[i_object].is_free = true;
    }

    // delete default, copy/move ctors as well as copy/move assignment op's.
    // this avoids unintended copy/move construction, as well as copy/move assignment
    MemPool() = delete;
    MemPool(const MemPool&) = delete;
    MemPool(const MemPool&&) = delete;
    MemPool& operator=(const MemPool&) = delete;
    MemPool& operator=(const MemPool&&) = delete;

    // todo: remove this method in non-testing builds
    /** Counts the number of free blocks. This is currently for testing only. */
    inline auto get_n_blocks_free() const noexcept {
        int n_free{ };
        for (const auto& b: blocks) {
            if (b.is_free)
                ++n_free;
        }
        return n_free;
    }

private:
    /**
     * @brief Update the index to the next available block
     * @details The best performing implementation method of this function depends on the
     * application. One should measure the performance in practice and see which works
     * best.
     */
    auto update_next_free_index() noexcept {
        const auto i = i_next_free;
        while (!blocks[i_next_free].is_free) {
            ++i_next_free;
            if (i_next_free == blocks.size()) [[unlikely]] {
                // the hardware branch predictor should always predict this not taken
                // however, a different method would be to have two while loops: one until
                // i_next_free == blocks.size(), and the other from 0 onward. this would
                // negate the need for this branch entirely
                i_next_free = 0;
            }
            if (i == i_next_free) [[unlikely]] {
                // there are better methods to handle this in production which are out of the
                // scope of this exercise.
                ASSERT(i != i_next_free, "MemPool::memory pool overrun");
            }
        }
    }

    struct Block {
        T object; // the actual stored object
        bool is_free{ true }; // true when available for allocation
    };

    std::vector<Block> blocks;
    size_t i_next_free{ 0 };
};
}
