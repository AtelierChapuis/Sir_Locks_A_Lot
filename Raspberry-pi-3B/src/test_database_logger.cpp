#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "database_logger.hpp"

class DatabaseLoggerTest : public ::testing::Test {
protected:
    std::string test_db_path = "test_door_events.db";
    std::string test_log_file;
    
    void SetUp() override {
        // Get current date for log file name
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        test_log_file = ss.str() + ".txt";
        
        // Remove test files if they exist
        std::filesystem::remove(test_db_path);
        std::filesystem::remove(test_log_file);
    }
    
    void TearDown() override {
        std::filesystem::remove(test_db_path);
        std::filesystem::remove(test_log_file);
    }
};

TEST_F(DatabaseLoggerTest, OpenAndClose) {
    DatabaseLogger logger(test_db_path);
    
    EXPECT_NO_THROW(logger.open());
    EXPECT_NO_THROW(logger.close());
    
    // Database file should exist
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
}

TEST_F(DatabaseLoggerTest, LogEvent) {
    DatabaseLogger logger(test_db_path);
    logger.open();
    
    EXPECT_NO_THROW(logger.logEvent("locked", "stm32"));
    EXPECT_NO_THROW(logger.logEvent("unlocked", "laptop"));
    
    logger.close();
    
    // Check that log file was created
    EXPECT_TRUE(std::filesystem::exists(test_log_file));
}

TEST_F(DatabaseLoggerTest, LogMultipleEvents) {
    DatabaseLogger logger(test_db_path);
    logger.open();
    
    for (int i = 0; i < 10; ++i) {
        std::string state = (i % 2 == 0) ? "locked" : "unlocked";
        EXPECT_NO_THROW(logger.logEvent(state, "test_source"));
    }
    
    logger.close();
}