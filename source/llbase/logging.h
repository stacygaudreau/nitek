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


namespace LL
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
        thread = create_and_start_thread(
                -1, "<LL::Logger>", [this]() { process_queue(); });
        ASSERT(thread != nullptr, "<Logger> failed to start thread");
    }

    ~Logger() {
        std::string time_str;
        std::cerr << get_time_str(&time_str) << " <Logger> flush and close logfile " <<
                  filename << std::endl;
        // wait for the logging queue to be emptied out by its dedicated thread
        while (queue.size()) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(500ms);
        }
        // clean up after all logging events in the queue have been processed
        is_running = false;
        thread->join();
        file.close();
        std::cerr << get_time_str(&time_str) << " <Logger> exiting logger for logfile "
                  << filename << std::endl;
    }

    /**
     * @brief Process data in the logging queue, flushing it to file.
     */
    void process_queue() noexcept {
        while (is_running) {
            for (auto next = queue.get_next_to_read();
                 queue.size() && next;
                 next = queue.get_next_to_read()) {
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

    /**
     * @brief Enqueues a LogElement to the next block in the logging queue.
     * @param log_element A LogElement primitive to enqueue.
     * @details Called by other overloads of push_value() in order to enqueue
     * various types of data into the logging queue.
     */
    void push_element(const LogElement& log_element) noexcept {
        *(queue.get_next_to_write()) = log_element;
        queue.increment_write_index();
    }

    /**
     * @brief Enqueue a single char value to the logging queue.
     */
    void push_value(const char value) noexcept {
        push_element(
                LogElement{ LogType::CHAR, { .c = value }});
    }
    /**
     * @brief Enqueue a collection of char values to the logger's queue.
     * @details This method can potentially be improved by using memcpy()
     * to copy all chars in the array instead of iterating.
     */
    void push_value(const char* value) noexcept {
        while (*value) {
            push_value(*value);
            ++value;
        }
    }
    /**
     * @brief Enqueue a std::string to the logging queue.
     */
    void push_value(const std::string& value) noexcept {
        push_value(value.c_str());
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const int value) noexcept {
        push_element(
                LogElement{ LogType::INT, { .i = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const long value) noexcept {
        push_element(
                LogElement{ LogType::LONG, { .l = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const long long value) noexcept {
        push_element(
                LogElement{ LogType::LONG_LONG, { .ll = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const unsigned value) noexcept {
        push_element(
                LogElement{ LogType::U_INT, { .u = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const unsigned long value) noexcept {
        push_element(
                LogElement{ LogType::U_LONG, { .ul = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const unsigned long long value) noexcept {
        push_element(
                LogElement{ LogType::U_LONG_LONG, { .ull = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(const float value) noexcept {
        push_element(
                LogElement{ LogType::FLOAT, { .f = value }});
    }
    /**
     * @brief Enqueue a single value to the logging queue.
     */
    void push_value(double value) noexcept {
        push_element(
                LogElement{ LogType::DOUBLE, { .d = value }});
    }

    /**
     * @brief Log message to file, using printf-like syntax. Like printf(), each % symbol
     * in a string will be replaced by any number of corresponding arguments given.
     * @details Use %% to escape the % character.
     * @param s Logging message/string with % to replace by any arguments which follow.
     */
    template<typename T, typename... A>
    void logf(const char* s, const T& value, A... args) noexcept {
        while (*s) {
            if (*s == '%') {
                if (*(s + 1) == '%') [[unlikely]] { // allow %% to escape %
                    ++s;
                }
                else {
                    push_value(value);  // % is substituted with argument value
                    logf(s + 1, args...);   // recursive call with next argument
                    return;
                }
            }
            push_value(*s++);
        }
        FATAL("<Logger::logf()> too many arguments provided");
    }

    /**
     * @brief Log message to file, using printf()-like syntax.
     * @param s Logging message/string with % to replace by any arguments which follow.
     */
    void logf(const char* s) noexcept {
        // base case to handle when there are no arguments specified and it's
        // just a textual log message. We do not use template specialisation here
        // since it will not inline in GCC
        while (*s) {
            if (*s == '%') {
                if (*(s + 1) == '%') [[unlikely]] { // allow %% to escape %
                    ++s;
                }
                else {
                    FATAL("<Logger::logf()> missing arguments");
                }
            }
            push_value(*s++);
        }
    }

private:
    const std::string filename; // log output file name
    std::ofstream file; // log output file stream
    LFQueue<LogElement> queue;  // log events pending write to file
    std::atomic<bool> is_running{ true };  // for stopping the process
    std::unique_ptr<std::thread> thread{ nullptr }; // dedicated logging thread

DELETE_DEFAULT_COPY_AND_MOVE(Logger)
};

}