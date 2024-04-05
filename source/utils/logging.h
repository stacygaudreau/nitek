/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file logging.h
 *  @brief Logging utilities for low-latency applications
 *  @author Stacy Gaudreau
 *  @date 2024.04.04
 *
 */


#pragma once


#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <atomic>
#include <memory>
#include "macros.h"
#include "lfqueue.h"
#include "threading.h"
#include "timekeeping.h"


namespace Utils
{
constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

/** @brief The type of data stored in a LogElement */
enum class LogType : int8_t {
    CHAR,
    INT,
    LONG,
    LONG_LONG,
    U_INT,
    U_LONG,
    U_LONG_LONG,
    FLOAT,
    DOUBLE,
};

/** @brief An element to be pushed into the logger queue */
struct LogElement {
    LogType type{ LogType::CHAR }; // the type of value held
    // we could have used std::variant here for better type safety, but
    //  std::variant has bad runtime performance compared to union
    union {
        char c;

        int i;
        long l;
        long long ll;

        unsigned u;
        unsigned long ul;
        unsigned long long ull;

        float f;
        double d;
    } value;
};


class Logger final {
public:

    explicit Logger(const std::string& output_filename)
            : filename(output_filename),
              queue(LOG_QUEUE_SIZE) {
        file.open(filename);
        ASSERT(file.is_open(), "<Logger> could not open output logfile "
                + output_filename);
        thread = create_and_start_thread(-1, "<Utils::Logger>", [this]() { flush_queue(); });
        ASSERT(thread != nullptr, "<Logger> failed to start thread");
    }

    ~Logger() {
        std::string time_str;
        std::cerr << get_time_str(&time_str) << " <Logger> flush and close logfile " <<
                  filename << std::endl;
        while (queue.size()) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(500ms);
        }
        is_running = false;
        thread->join();
        file.close();
        std::cerr << get_time_str(&time_str) << " <Logger> exiting logger for logfile "
                  << filename << std::endl;
    }

    /**
     * @brief Empties the logging queue of data and writes it to file.
     */
    void flush_queue() noexcept {
        while (is_running) {
            for (auto next = queue.get_next_to_read(); queue.size() && next; next = queue
                    .get_next_to_read()) {
                switch (next->type) {
                case LogType::CHAR:
                    file << next->value.c;
                    break;
                case LogType::INT:
                    file << next->value.i;
                    break;
                case LogType::LONG:
                    file << next->value.l;
                    break;
                case LogType::LONG_LONG:
                    file << next->value.ll;
                    break;
                case LogType::U_INT:
                    file << next->value.u;
                    break;
                case LogType::U_LONG:
                    file << next->value.ul;
                    break;
                case LogType::U_LONG_LONG:
                    file << next->value.ull;
                    break;
                case LogType::FLOAT:
                    file << next->value.f;
                    break;
                case LogType::DOUBLE:
                    file << next->value.d;
                    break;
                }
                queue.increment_read_index();
            }
            file.flush();
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }

    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger(const Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(const Logger&&) = delete;

private:
    const std::string filename; // log output file name
    std::ofstream file; // log output file stream 
    LFQueue<LogElement> queue;  // log events pending write to file
    std::atomic<bool> is_running{ true };  // for stopping the process
    std::unique_ptr<std::thread> thread{ nullptr }; // separate logging thread
};

}