cmake_minimum_required(VERSION 3.10)
project(DoorRelayService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Main executable
add_executable(door_relay_service
    main.cpp
)

target_include_directories(door_relay_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(door_relay_service
    SQLite::SQLite3
    Threads::Threads
)

# Enable testing
enable_testing()
find_package(GTest REQUIRED)

# Test executables
add_executable(test_json_parser test_json_parser.cpp)
target_include_directories(test_json_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_json_parser GTest::GTest GTest::Main)

add_executable(test_database_logger test_database_logger.cpp)
target_include_directories(test_database_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_database_logger 
    GTest::GTest 
    GTest::Main
    SQLite::SQLite3
    stdc++fs
)

# Add tests
add_test(NAME JsonParserTest COMMAND test_json_parser)
add_test(NAME DatabaseLoggerTest COMMAND test_database_logger)

# File: Makefile (alternative to CMake)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lsqlite3 -lpthread

SOURCES = main.cpp
EXECUTABLE = door_relay_service

TEST_SOURCES = test_json_parser.cpp test_database_logger.cpp
TEST_EXECUTABLES = test_json_parser test_database_logger

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_json_parser: test_json_parser.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

test_database_logger: test_database_logger.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lsqlite3 -lstdc++fs -lpthread

tests: $(TEST_EXECUTABLES)

clean:
	rm -f $(EXECUTABLE) $(TEST_EXECUTABLES)

.PHONY: all tests clean