#include "gtest/gtest.h"
#include <iostream>
#include <thread>
#include <vector>
#include "llbase/mempool.h"


using namespace LL;


class MemPoolBasics : public ::testing::Test {
protected:
    // test data structure to store in pool
    struct Data {
        int d[3];
    };
    static constexpr int N_BLOCKS{ 32 };
    MemPool<double> double_pool{ N_BLOCKS };
    MemPool<Data> data_pool{ N_BLOCKS };
    void SetUp() override { }
    void TearDown() override { }
};


TEST_F(MemPoolBasics, empty_mempool_has_max_free_blocks) {
    ASSERT_EQ(double_pool.get_n_blocks_free(), N_BLOCKS);
}

TEST_F(MemPoolBasics, mempool_asserts_before_overrun) {
    MemPool<uint64_t> pool{ 1 };
    ASSERT_DEATH(pool.allocate(1), ".*overrun.*");
}

TEST_F(MemPoolBasics, allocating_doubles) {
    // allocate and accumulate a bunch of double values,
    // reading them as a test along the way
    double sum{ 0.0 };
    for (size_t i{ }; i < N_BLOCKS - 1; ++i) {
        auto d = double_pool.allocate(double(i));
        sum += *d;
    }
    ASSERT_TRUE(sum > 0.0);
    ASSERT_EQ(double_pool.get_n_blocks_free(), 1);
}

TEST_F(MemPoolBasics, deallocating_doubles) {
    // allocate and deallocate a bunch of doubles
    for (size_t i{ }; i < N_BLOCKS - 1; ++i) {
        auto d = double_pool.allocate(double(i));
        double_pool.deallocate(d);
    }
    ASSERT_EQ(double_pool.get_n_blocks_free(), N_BLOCKS);
}

TEST_F(MemPoolBasics, allocating_non_primitive_data) {
    // allocate a non-primitive data structure
    auto last = data_pool.allocate(Data({ 1, 2, 3 }));
    for (size_t i{ }; i < N_BLOCKS - 2; ++i) {
        last = data_pool.allocate(Data({ 1, 2, 3 }));
    }
    ASSERT_EQ(last->d[0], 1);
    ASSERT_EQ(last->d[1], 2);
    ASSERT_EQ(last->d[2], 3);
    ASSERT_EQ(data_pool.get_n_blocks_free(), 1);
}