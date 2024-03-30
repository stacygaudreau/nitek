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
