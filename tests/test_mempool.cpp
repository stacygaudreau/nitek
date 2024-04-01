#include <catch2/catch_test_macros.hpp>
#include "utils/threading.h"
#include <iostream>
#include <cstdint>
#include <thread>
#include <numeric>
#include <vector>

#include "utils/mempool.h"


// test data structure to store in pool
struct Data {
    int d[3];
};

TEST_CASE("MemPool basics", "[mempool]") {
    using namespace Utils;
    constexpr int N_BLOCKS{ 32 };
    MemPool<double> double_pool{ N_BLOCKS };
    MemPool<Data> data_pool{ N_BLOCKS };

    SECTION("empty mempool has maximum free blocks") {
        REQUIRE(double_pool.get_n_blocks_free() == N_BLOCKS);
    }

    SECTION("allocating doubles") {
        // allocate and accumulate a bunch of double values,
        // reading them as a test along the way
        double sum{ 0.0 };
        for (size_t i{ }; i < N_BLOCKS - 1; ++i) {
            auto d = double_pool.allocate(double(i));
            sum += *d;
        }
        REQUIRE(sum > 0.0);
        REQUIRE(double_pool.get_n_blocks_free() == 1);
    }

    SECTION("deallocating doubles") {
        // allocate and accumulate a bunch of double values,
        // reading them as a test along the wa
        double sum{ 0.0 };
        for (size_t i{ }; i < N_BLOCKS - 1; ++i) {
            auto d = double_pool.allocate(double(i));
            sum += *d;
        }
        REQUIRE(sum > 0.0);
        REQUIRE(double_pool.get_n_blocks_free() == 1);
    }

    // todo: rewrite with GTest ASSERT_DEATH
//    SECTION("empty mempool fails to allocate space") {
//        MemPool<uint64_t> pool{ 0 };
//        auto d = pool.allocate(2);
//        *d = 2;
//    }


}



