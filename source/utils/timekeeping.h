/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file timekeeping.h
 *  @brief Various timekeeping helpers
 *  @author Stacy Gaudreau
 *  @date 2024.04.04
 *
 */


#pragma once


#include <chrono>
#include <ctime>
#include <cstdint>
#include <string>


namespace Utils
{
using Nanos = int64_t;

constexpr Nanos NANOS_TO_MICROS{ 1000 };
constexpr Nanos MICROS_TO_MILLIS{ 1000 };
constexpr Nanos MILLIS_TO_SECS{ 1000 };
constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

/** @brief Get the current time, in nanoseconds */
inline auto get_time_nanos() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

/** @brief Get the current time, as a string */
inline auto& get_time_str(std::string* time_str) {
    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    time_str->assign(ctime(&time));
    if (!time_str->empty())
        time_str->at(time_str->length() - 1) = '\0';
    return *time_str;
}
}
