/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file macros.h
 *  @brief Macro helpers for various things
 *  @author Stacy Gaudreau
 *  @date 2024.03.30
 *
 */


#pragma once


#include <iostream>
#include <string>
#include <vector>


inline auto ASSERT(bool cond, const std::string& msg) noexcept {
    // todo: add debug/release flag to optimise this out in release builds
    if (!cond) [[unlikely]] {
        std::cerr << msg << "\n";
        exit(EXIT_FAILURE);
    }
}

inline auto FATAL(const std::string& msg) noexcept {
    std::cerr << msg << "\n";
    exit(EXIT_FAILURE);
}

/**
 * @brief Check if an element does not exist in a vector
 * @return True when the element is not in the vector
 */
template<typename T>
inline bool element_does_not_exist(std::vector<T>& v, T element) {
    return std::find(v.begin(), v.end(), element) == v.end();
}

/**
 * @brief Delete default ctor and copy/move ctor and assignment
 * operators for a given classname
 */
#define DELETE_DEFAULT_COPY_AND_MOVE(ClassName) \
public: \
    ClassName() = delete; \
    ClassName(const ClassName&) = delete; \
    ClassName(const ClassName&&) = delete; \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&&) = delete;

/**
 * @brief Qualify class members as public if under test, else private.
 */
#ifdef IS_TEST_SUITE
#define PRIVATE_IN_PRODUCTION public:
#else
#define PRIVATE_IN_PRODUCTION private:
#endif
