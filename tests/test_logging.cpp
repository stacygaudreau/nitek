#include "gtest/gtest.h"
#include <string>
#include <iostream>
#include "utils/logging.h"


using namespace Utils;


class LoggingBasics : public ::testing::Test {
protected:
    std::string filename{ "test.log" };
    void SetUp() override { }
    void TearDown() override { }
};


TEST_F(LoggingBasics, logger_is_instantiated) {
    // a logger is constructed and the output file is opened successfully
    Logger logger{ filename };
    ASSERT_TRUE(&logger);
}

