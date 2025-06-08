cmake_minimum_required(VERSION 3.10)
project(DoorControlClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Threads REQUIRED)

# Main executable
add_executable(door_control_client
    laptop_main.cpp
)

target_include_directories(door_control_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(door_control_client
    Threads::Threads
)

# Enable testing
enable_testing()
find_package(GTest REQUIRED)

# Test executable
add_executable(test_door_control_client test_door_control_client.cpp)
target_include_directories(test_door_control_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_door_control_client GTest::GTest GTest::Main)

# Add tests
add_test(NAME DoorControlClientTest COMMAND test_door_control_client)

# File: Makefile (alternative to CMake for laptop client)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lpthread

SOURCES = laptop_main.cpp
EXECUTABLE = door_control_client

TEST_SOURCES = test_door_control_client.cpp
TEST_EXECUTABLE = test_door_control_client

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_EXECUTABLE): $(TEST_SOURCES)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

clean:
	rm -f $(EXECUTABLE) $(TEST_EXECUTABLE)

.PHONY: all test clean