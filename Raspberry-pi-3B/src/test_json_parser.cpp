#include <gtest/gtest.h>
#include "json_parser.hpp"

TEST(JsonParserTest, ParseSimpleObject) {
    std::string json = R"({"key1":"value1","key2":"value2"})";
    auto obj = SimpleJSON::parse(json);
    
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj["key1"], "value1");
    EXPECT_EQ(obj["key2"], "value2");
}

TEST(JsonParserTest, ParseWithWhitespace) {
    std::string json = R"({
        "source" : "stm32",
        "event" : "door_locked",
        "timestamp" : "2025-06-08T10:25:00Z"
    })";
    
    auto obj = SimpleJSON::parse(json);
    
    EXPECT_EQ(obj["source"], "stm32");
    EXPECT_EQ(obj["event"], "door_locked");
    EXPECT_EQ(obj["timestamp"], "2025-06-08T10:25:00Z");
}

TEST(JsonParserTest, StringifyObject) {
    SimpleJSON::Object obj;
    obj["type"] = "ack";
    obj["status"] = "ok";
    
    std::string json = SimpleJSON::stringify(obj);
    
    // Order might vary, so check both possibilities
    EXPECT_TRUE(json == R"({"type":"ack","status":"ok"})" || 
                json == R"({"status":"ok","type":"ack"})");
}

TEST(JsonParserTest, ParseEmptyObject) {
    std::string json = "{}";
    auto obj = SimpleJSON::parse(json);
    
    EXPECT_EQ(obj.size(), 0);
}