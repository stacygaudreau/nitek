#include "gtest/gtest.h"
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "llbase/logging.h"


using namespace LL;
namespace fs = std::filesystem;


class LoggingBasics : public ::testing::Test {
protected:
    std::string filename{ "test.log" };
    fs::path filepath{ filename };
    std::chrono::milliseconds t_wait{ 30 }; // queue processing could take ~10ms so we wait for 30

    // delete logfile, if it exists
    void delete_existing_logfile() {
        if (fs::exists(filepath)) {
            fs::remove(filepath);
        }
    }

    // opens and reads the logfile's last line
    std::string read_last_line_of_logfile() {
        std::string line{ }, last_line{ };
        if (!fs::exists(filepath)) {
            std::cerr << "Logfile does not exist!";
            return last_line;
        }
        std::ifstream f{ filepath };
        if (!f.is_open()) {
            std::cerr << "Failed to open logfile.";
            return last_line;
        }
        while (getline(f, line)) {
            if (!line.empty()) {
                last_line = line;
            }
        }
        f.close();
        return last_line;
    }

    void SetUp() override {
        delete_existing_logfile();
    }
    void TearDown() override {
        delete_existing_logfile();
    }
};


TEST_F(LoggingBasics, is_constructed) {
    // a logger is constructed without err and the logfile exists on disk
    Logger logger{ filename };
    ASSERT_TRUE(&logger);
    ASSERT_TRUE(fs::exists(filepath));
}

TEST_F(LoggingBasics, writes_primitive_element_to_logfile) {
    // the logging queue is processed and writes a primitive LogElement to file
    Logger logger{ filename };
    const char char_to_write{ 'p' };
    LogElement element{ LogType::CHAR, { .c=char_to_write }};
    logger.push_element(element);
    std::this_thread::sleep_for(t_wait);
    const std::string line_expected{ char_to_write };
    EXPECT_EQ(line_expected, read_last_line_of_logfile());
}

TEST_F(LoggingBasics, logging_a_c_style_string) {
    // a C-style string is logged to file
    Logger logger{ filename };
    const char* s{ "test c-style string" };
    const std::string expected{ "log entry: " + std::string{ s }};
    logger.logf("log entry: %", s);
    std::this_thread::sleep_for(t_wait);
    EXPECT_EQ(read_last_line_of_logfile(), expected);
}

TEST_F(LoggingBasics, logging_a_std_string) {
    // a std::string is logged to file
    Logger logger{ filename };
    const std::string s{ "test string" };
    const std::string expected{ "log entry: " + s };
    logger.logf("log entry: %", s);
    std::this_thread::sleep_for(t_wait);
    EXPECT_EQ(read_last_line_of_logfile(), expected);
}

TEST_F(LoggingBasics, logging_integers) {
    // all types of integers are logged to file by the logf method
    int i{ -2 };
    long l{ -8 };
    long long ll{ -16 };
    unsigned u{ 32 };
    unsigned long ul{ 64 };
    unsigned long long ull{ 128 };
    Logger logger{ filename };
    const std::string expected{ "logged integers: -2 -8 -16 32 64 128" };
    logger.logf("logged integers: % % % % % %", i, l, ll, u, ul, ull);
    std::this_thread::sleep_for(t_wait);
    EXPECT_EQ(read_last_line_of_logfile(), expected);
}

TEST_F(LoggingBasics, logging_floats) {
    // floats and doubles are logged to file using the logf method
    float f{ 3.14 };
    double d{ 123.124 };
    Logger logger{ filename };
    const std::string expected{ "logged floats: 3.14 123.124" };
    logger.logf("logged floats: % %", f, d);
    std::this_thread::sleep_for(t_wait);
    EXPECT_EQ(read_last_line_of_logfile(), expected);
}

TEST_F(LoggingBasics, logging_a_complex_message) {
    // a complex message with numerous types is logged to file, including an escaped '%' char
    unsigned y{ 2023 };
    unsigned M{ 10 };
    unsigned d{ 20 };
    unsigned m{ 60 };
    std::string symbol{ "SPY" };
    float price{ 510.23 };
    float percent{ 55.23 };

    const std::string expected{ "2023.10.20.60 <Order::Execution>: SPY order filled at 510.23."
                                " % filled: 55.23" };
    Logger logger{ filename };
    logger.logf("%.%.%.% <Order::Execution>: % order filled at %. %% filled: %", y, M, d, m,
                symbol, price, percent);
    std::this_thread::sleep_for(t_wait);
    EXPECT_EQ(read_last_line_of_logfile(), expected);
}

