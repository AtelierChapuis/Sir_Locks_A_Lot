#include <gtest/gtest.h>
#include "json_parser.hpp"

// Mock TCP Client for testing
class MockTcpClient {
private:
    std::queue<std::string> read_buffer_;
    std::queue<std::string> write_buffer_;
    bool connected_;
    
public:
    MockTcpClient(const std::string&, int) : connected_(false) {}
    
    void connect() { connected_ = true; }
    void disconnect() { connected_ = false; }
    bool isConnected() const { return connected_; }
    
    std::string readLine() {
        if (!read_buffer_.empty()) {
            std::string line = read_buffer_.front();
            read_buffer_.pop();
            return line;
        }
        return "";
    }
    
    void writeLine(const std::string& data) {
        write_buffer_.push(data);
    }
    
    // Test helpers
    void addToReadBuffer(const std::string& data) {
        read_buffer_.push(data);
    }
    
    std::string getLastWritten() {
        if (!write_buffer_.empty()) {
            std::string data = write_buffer_.front();
            write_buffer_.pop();
            return data;
        }
        return "";
    }
};

TEST(DoorControlClientTest, ParseLockCommand) {
    SimpleJSON::Object cmd;
    cmd["command"] = "lock";
    cmd["source"] = "laptop";
    
    std::string json = SimpleJSON::stringify(cmd);
    auto parsed = SimpleJSON::parse(json);
    
    EXPECT_EQ(parsed["command"], "lock");
    EXPECT_EQ(parsed["source"], "laptop");
}

TEST(DoorControlClientTest, ParseSyncResponse) {
    std::string response = R"({"type":"sync_response","door_state":"locked","last_source":"stm32"})";
    auto parsed = SimpleJSON::parse(response);
    
    EXPECT_EQ(parsed["type"], "sync_response");
    EXPECT_EQ(parsed["door_state"], "locked");
    EXPECT_EQ(parsed["last_source"], "stm32");
}

TEST(DoorControlClientTest, ParseDoorEvent) {
    std::string event = R"({"source":"stm32","event":"door_unlocked","timestamp":"2025-06-08T10:25:00Z"})";
    auto parsed = SimpleJSON::parse(event);
    
    EXPECT_EQ(parsed["source"], "stm32");
    EXPECT_EQ(parsed["event"], "door_unlocked");
    EXPECT_EQ(parsed["timestamp"], "2025-06-08T10:25:00Z");
}