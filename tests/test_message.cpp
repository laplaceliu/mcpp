#include "gtest/gtest.h"
#include "mcpp/protocol/message.hpp"

using namespace mcpp;

TEST(MessageParserTest, ParseRequest) {
    std::string data = R"({"jsonrpc": "2.0", "method": "test", "id": 1})";
    auto result = MessageParser::parse(data);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().type(), MessageType::Request);
    EXPECT_EQ(result.value().request.method, "test");
}

TEST(MessageParserTest, ParseNotification) {
    std::string data = R"({"jsonrpc": "2.0", "method": "notify"})";
    auto result = MessageParser::parse(data);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().type(), MessageType::Notification);
    EXPECT_EQ(result.value().request.method, "notify");
}

TEST(MessageParserTest, ParseResponse) {
    std::string data = R"({"jsonrpc": "2.0", "result": {"data": "ok"}, "id": 1})";
    auto result = MessageParser::parse(data);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().type(), MessageType::Response);
    EXPECT_FALSE(result.value().response.is_error);
}

TEST(MessageParserTest, ParseErrorResponse) {
    std::string data = R"({"jsonrpc": "2.0", "error": {"code": -32600, "message": "Invalid Request"}, "id": 1})";
    auto result = MessageParser::parse(data);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().type(), MessageType::Response);
    EXPECT_TRUE(result.value().response.is_error);
}

TEST(MessageSerializerTest, SerializeRequest) {
    JsonRpcRequest req;
    req.jsonrpc = "2.0";
    req.method = "test";
    req.id = 1;

    std::string serialized = MessageSerializer::serialize(req);
    EXPECT_NE(serialized.find("test"), std::string::npos);
    EXPECT_NE(serialized.find("2.0"), std::string::npos);
}

TEST(MessageSerializerTest, SerializeResponse) {
    JsonRpcResponse resp;
    resp.id = 1;
    resp.is_error = false;
    resp.result = JsonValue::object({{"data", "ok"}});

    std::string serialized = MessageSerializer::serialize(resp);
    EXPECT_NE(serialized.find("result"), std::string::npos);
}

TEST(MessageSerializerTest, RoundTrip) {
    std::string original = R"({"jsonrpc": "2.0", "method": "test", "params": {"a": 1}, "id": 1})";
    auto parse_result = MessageParser::parse(original);

    ASSERT_TRUE(parse_result.ok());
    auto serialized = MessageSerializer::serialize(parse_result.value().request);

    auto reparsed = MessageParser::parse(serialized);
    ASSERT_TRUE(reparsed.ok());
    EXPECT_EQ(reparsed.value().request.method, "test");
}
