/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file lfqueue.h
 *  @brief Low latency, lock-free queues
 *  @author Stacy Gaudreau
 *  @date 2024.04.02
 *
 */


#pragma once


#include <iostream>
#include <vector>
#include <atomic>
#include "macros.h"


namespace Utils
{
/**
 * @brief A low-latency lock-free queue for SPSC *only*.
 * @tparam T Type of object to store in the queue.
 * @details This class is not resizable at runtime, but an alternate version could be made
 * which does indeed resize. Memory is allocated on the heap at runtime, ideally
 * before any critcal paths are entered. An alternate approach which allocates stack memory
 * could be devised with a std::array instead of vector.
 */
template<typename T>
class LFQueue final {
public:
    /**
     * @brief Low latency lock-free queue of max length n_blocks.
     * @param n_blocks Max number of blocks the queue can hold.
     * @details SPSC (only). Single thread writing, single thread reading.
     */
    LFQueue(size_t n_blocks) : blocks(n_blocks, T{ }) { }

    /**
     * @brief Get pointer to the next object to write to
     * @return A pointer to the next object in the queue to write to
     */
    auto get_next_to_write() noexcept -> T* {
        return &blocks[i_write];
    }
    /**
     * @brief Increment the write index to the next block in the queue.
     */
    void increment_write_index() noexcept {
        i_write = (i_write + 1) % blocks.size();
        n_blocks++;
    }

    /**
     * @brief Get pointer to read next element, returning nullptr if there's nothing
     * to consume.
     */
    auto get_next_to_read() const noexcept -> const T* {
        return (size() ? &blocks[i_read] : nullptr);
    }
    /**
     * @brief Advance the read index to the next element in the queue.
     */
    void increment_read_index() noexcept {
        i_read = (i_read + 1) % blocks.size();
        ASSERT(n_blocks != 0, "LFQueue::read an invalid element in thread with id: "
                + std::to_string(pthread_self()));
        n_blocks--;
    }

    /** @brief Get the number of elements currently in the queue. */
    auto size() const noexcept {
        return n_blocks.load();
    }

    // delete default, copy/move ctors as well as copy/move assignment op's.
    // this avoids unintended copy/move construction, as well as copy/move assignment
    LFQueue() = delete;
    LFQueue(const LFQueue&) = delete;
    LFQueue(const LFQueue&&) = delete;
    LFQueue& operator=(const LFQueue&) = delete;
    LFQueue& operator=(const LFQueue&&) = delete;

private:
    std::vector<T> blocks;
    std::atomic<size_t> n_blocks{ 0 };
    std::atomic<size_t> i_write{ 0 };
    std::atomic<size_t> i_read{ 0 };
};
};


