#include "gtest/gtest.h"
#include "core/json.hpp"

using namespace mcpp;

TEST(JsonTest, ParseObject) {
    Json json = Json::parse(R"({"key": "value"})");
    EXPECT_EQ(json["key"].get<std::string>(), "value");
}

TEST(JsonTest, ParseArray) {
    Json json = Json::parse(R"([1, 2, 3])");
    EXPECT_EQ(json[0].get<int>(), 1);
    EXPECT_EQ(json[1].get<int>(), 2);
    EXPECT_EQ(json[2].get<int>(), 3);
}

TEST(JsonTest, Dump) {
    Json json = Json::object();
    json["name"] = "test";
    json["value"] = 42;

    std::string str = json.dump();
    EXPECT_NE(str.find("name"), std::string::npos);
    EXPECT_NE(str.find("test"), std::string::npos);
}

TEST(JsonTest, ArrayAccess) {
    Json json = Json::array();
    json.append(1);
    json.append(2);
    json.append(3);

    EXPECT_EQ(json.size(), 3);
    EXPECT_EQ(json[0].get<int>(), 1);
}

TEST(JsonTest, ObjectAccess) {
    Json json = Json::object();
    json["key1"] = "value1";
    json["key2"] = "value2";

    EXPECT_TRUE(json.contains("key1"));
    EXPECT_FALSE(json.contains("nonexistent"));
    EXPECT_EQ(json["key1"].get<std::string>(), "value1");
}
