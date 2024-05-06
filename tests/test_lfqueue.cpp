#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include "llbase/lfqueue.h"
#include "llbase/threading.h"


using namespace LL;

struct Data {
    int d[3];
};

void consumer_read_data(LFQueue<Data>* ds) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(100ms);  // slow down thread; wait for enqueues
    while (ds->size() > 5) {
        const auto d = ds->get_next_to_read();
        ds->increment_read_index();
        (void) d;
    }
}


class LFQueueBasics : public ::testing::Test {
protected:
    static constexpr size_t N_BLOCKS{ 32 };
    const Data d{ 1, 2, 3 };
    LFQueue<Data> ds{ N_BLOCKS };

    void SetUp() override { }
    void TearDown() override { }
};


TEST_F(LFQueueBasics, queue_is_instantiated) {
    ASSERT_EQ(ds.size(), 0);
}

TEST_F(LFQueueBasics, elements_added_to_queue) {
    *(ds.get_next_to_write()) = d;
    ds.increment_write_index();
    ASSERT_EQ(ds.size(), 1);
}

TEST_F(LFQueueBasics, elements_are_read_and_dequeued) {
    const Data d{ 1, 2, 3 };
    *(ds.get_next_to_write()) = d;
    ds.increment_write_index();
    ASSERT_EQ(ds.size(), 1);
    auto d_read = ds.get_next_to_read();
    ds.increment_read_index();
    ASSERT_EQ(ds.size(), 0);
    ASSERT_EQ(d_read->d[0], 1);
    ASSERT_EQ(d_read->d[1], 2);
    ASSERT_EQ(d_read->d[2], 3);
}

TEST_F(LFQueueBasics, multithreaded_spsc_queue_consumption) {
    // single producer, single consumer threaded use of queue
    // combining threading utils with lfqueue
    EXPECT_EQ(ds.size(), 0);
    // first 10 elements enqueued by main thread with no consumer running
    for (auto i{ 0 }; i < 10; ++i) {
        const Data d{ i, i * 10, i * 100 };
        *(ds.get_next_to_write()) = d;
        ds.increment_write_index();
    }
    EXPECT_EQ(ds.size(), 10);
    // start the separate consumer thread
    auto consumer = create_and_start_thread(-1, "LFQueue::consumer", consumer_read_data, &ds);
    // enqueue some more elements while consumer is running
    for (auto i{ 10 }; i < 15; ++i) {
        const Data d{ i, i * 10, i * 100 };
        *(ds.get_next_to_write()) = d;
        ds.increment_write_index();
    }
    consumer->join();
    EXPECT_EQ(ds.size(), 5);
}