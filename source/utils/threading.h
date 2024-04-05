/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file threads.h
 *  @brief Low latency multithreading utilities
 *  @author Stacy Gaudreau
 *  @date 2024.03.30
 *
 */

#pragma once


#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <unistd.h>
#include <sys/syscall.h>


namespace Utils
{

/**
 * @brief Pin the current thread to a specific core_id
 * @param core_id Core ID to pin the thread to
 * @return false if pinning core affinity fails
 */
inline auto pin_thread_to_core(int core_id) noexcept {
    // we are setting the allowed **set** of CPU cores a thread can run on
    cpu_set_t cpu_set;  // bitmask rep'n of a set of CPUs
    CPU_ZERO(&cpu_set); // zeros the mask
    CPU_SET(core_id, &cpu_set); // using bitwise operations, set *single* core for the thread
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set) == 0);
}

/**
 * @brief Create and start a POSIX thread, pinning to the specified core_id
 * @tparam T Type of callable object for the thread to work on
 * @tparam A Variadic parameter pack for the callable T
 * @param core_id Core ID to pin the thread to
 * @param name String identifier for the thread
 * @param fn The callable object for the thread to work on
 * @param args Zero or more arguments passed to the callable fn
 * @return The created thread, or nullptr if thread creation failed
 */
template<typename T, typename... A>
inline auto create_and_start_thread(int core_id, const std::string& name, T&& fn, A&& ...args)
noexcept {
    std::atomic<bool> running{ false }, failed{ false };

    // lambda of thread body, passed to ctor such that any kind of fn and arbitrary
    //  arguments/types can be passed to std::thread
    auto thread_body = [&] {
        // fail if the thread cannot be pinned to the specified core_id
        if (core_id >= 0 && !pin_thread_to_core(core_id)) {
            std::cerr << "<Threading> failed to set core affinity for " << name << " " <<
                      pthread_self() <<
                      " to " << core_id << "\n";
            failed = true;
            return;
        }
        std::cout << "<Threading> Setting core affinity for " << name << " " << pthread_self() <<
                  " to " <<
                  core_id << "\n";
        running = true;
        // perfect forwarding is used to pass fn's variadic l/rvalues
        std::forward<T>(fn)((std::forward<A>(args))...);
    };

    // instantiate thread, waiting for it to start or fail
    auto t = std::make_unique<std::thread>(thread_body);
    while (!(running || failed)) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(50ms);
    }
    if (failed) {
        t->join();
        t = nullptr;
    }
    return t;
}
}



