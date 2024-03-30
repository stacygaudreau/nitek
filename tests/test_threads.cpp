#include <catch2/catch_test_macros.hpp>
#include "utils/threading.h"
#include <iostream>
#include <thread>
#include <numeric>
#include <vector>


auto threaded_fn(bool sleep) {
    if (sleep) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(50ms);
    }
}

void partial_sum(const std::vector<int>& numbers, int start, int end, int* sum) {
    *sum = std::accumulate(numbers.begin() + start, numbers.begin() + end, 0);
}

TEST_CASE("threading basics", "[threads]") {
    using namespace Utils;

    SECTION("threads are instantiated and joinable") {
        auto t1 = create_and_start_thread(-1, "threadedFn1", threaded_fn, false);
        auto t2 = create_and_start_thread(1, "threadedFn2", threaded_fn, false);

        REQUIRE(t1->joinable());
        REQUIRE(t2->joinable());

        if (t1->joinable())
            t1->join();
        if (t2->joinable())
            t2->join();
    }

    SECTION("naive thread pool performs multithreaded accumulation",
            "[threads]") {
        std::vector<int> numbers(100);
        std::iota(numbers.begin(), numbers.end(), 1);
        const int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
        constexpr int n_threads{ 4 };
        size_t size_partial{ numbers.size() / n_threads };
        // a simple thread pool
        std::vector<std::unique_ptr<std::thread>> threads;
        std::vector<int> partial_sums(n_threads);

        // launch all threads
        for (size_t i{ }; i < n_threads; ++i) {
            const auto start = i * size_partial;
            auto end = (i + 1) * size_partial;
            // the final thread handles all remaining elements
            if (i == n_threads - 1) {
                end = numbers.size();
            }
            threads.emplace_back(create_and_start_thread(i - 1, "partialSum" + std::to_string
                    (i), partial_sum, std::cref(numbers), start, end, &partial_sums[i]));
        }
        // await all threads
        for (auto& t: threads) {
            t->join();
        }
        // sum all partials
        const auto res = std::accumulate(partial_sums.begin(), partial_sums.end(), 0);
        REQUIRE(res == sum);
    }
}