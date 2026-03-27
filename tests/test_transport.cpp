/**
 * @file test_transport.cpp
 * @brief Unit tests for transport layer
 */

#include "gtest/gtest.h"
#include "mcpp/transport/websocket.hpp"
#include "mcpp/transport/http.hpp"
#include "mcpp/transport/stdio.hpp"
#include "mcpp/transport/transport.hpp"

using namespace mcpp;

// ============ WebSocketFramer Tests ============

TEST(WebSocketFramerTest, FrameShortMessage) {
    std::string message = "hello";
    std::string framed = WebSocketFramer::frame(message);

    // Check frame header
    EXPECT_EQ(static_cast<uint8_t>(framed[0]), 0x81);  // FIN + text opcode

    // Length byte should have mask bit (0x80) set and length <= 125
    uint8_t second_byte = static_cast<uint8_t>(framed[1]);
    EXPECT_TRUE(second_byte & 0x80);  // Mask bit set
    EXPECT_EQ((second_byte & 0x7F), 5);  // Payload length = 5

    // After length byte, there should be 4-byte mask key
    EXPECT_EQ(framed.size(), 2 + 4 + message.size());
}

TEST(WebSocketFramerTest, FrameMediumMessage) {
    std::string message(200, 'x');  // 200 bytes > 125, < 65536
    std::string framed = WebSocketFramer::frame(message);

    uint8_t second_byte = static_cast<uint8_t>(framed[1]);
    EXPECT_TRUE(second_byte & 0x80);  // Mask bit set
    EXPECT_EQ((second_byte & 0x7F), 126);  // Extended length marker

    // Check 2-byte extended length
    EXPECT_EQ(static_cast<uint8_t>(framed[2]), 0x00);  // High byte
    EXPECT_EQ(static_cast<uint8_t>(framed[3]), 200);   // Low byte

    // Frame size = 1(header) + 1(len) + 2(ext_len) + 4(mask) + 200(payload) = 208
    EXPECT_EQ(framed.size(), 1 + 1 + 2 + 4 + 200);
}

TEST(WebSocketFramerTest, FrameLargeMessage) {
    std::string message(70000, 'y');  // 70000 bytes > 65536
    std::string framed = WebSocketFramer::frame(message);

    uint8_t second_byte = static_cast<uint8_t>(framed[1]);
    EXPECT_TRUE(second_byte & 0x80);  // Mask bit set
    EXPECT_EQ((second_byte & 0x7F), 127);  // 64-bit length marker

    // Check 8-byte extended length (big-endian for 70000 = 0x0000000000011170)
    // 70000 = 0x11170 (5 hex digits), as 64-bit: 0x0000000000011170
    // In big-endian bytes: 00 00 00 00 00 01 11 70
    EXPECT_EQ(static_cast<uint8_t>(framed[2]), 0x00);  // Byte 0
    EXPECT_EQ(static_cast<uint8_t>(framed[3]), 0x00);  // Byte 1
    EXPECT_EQ(static_cast<uint8_t>(framed[4]), 0x00);  // Byte 2
    EXPECT_EQ(static_cast<uint8_t>(framed[5]), 0x00);  // Byte 3
    EXPECT_EQ(static_cast<uint8_t>(framed[6]), 0x00);  // Byte 4
    EXPECT_EQ(static_cast<uint8_t>(framed[7]), 0x01);  // Byte 5
    EXPECT_EQ(static_cast<uint8_t>(framed[8]), 0x11);  // Byte 6
    EXPECT_EQ(static_cast<uint8_t>(framed[9]), 0x70);  // Byte 7

    // Frame size = 1 + 1 + 8 + 4 + 70000 = 70014
    EXPECT_EQ(framed.size(), 1 + 1 + 8 + 4 + 70000);
}

TEST(WebSocketFramerTest, FrameEmptyMessage) {
    std::string message = "";
    std::string framed = WebSocketFramer::frame(message);

    EXPECT_EQ(framed.size(), 1 + 1 + 4);  // header + len + mask (empty payload)
    EXPECT_EQ(static_cast<uint8_t>(framed[0]), 0x81);
}

TEST(WebSocketFramerTest, IsCompleteIncompleteFrame) {
    unsigned char data[10] = {0x81, 0x05};  // FIN + text, length 5, but no payload
    EXPECT_FALSE(WebSocketFramer::is_complete(reinterpret_cast<char*>(data), 2));
}

TEST(WebSocketFramerTest, IsCompleteCompleteFrame) {
    // Complete frame: 0x81 0x05 "hello"
    unsigned char data[10] = {0x81, 0x05, 'h', 'e', 'l', 'l', 'o'};
    EXPECT_TRUE(WebSocketFramer::is_complete(reinterpret_cast<char*>(data), 7));
}

TEST(WebSocketFramerTest, IsCompletePartialExtendedLength) {
    // 126 extended length needs 2 bytes
    unsigned char data[4] = {0x81, 0x7E, 0x00};  // Only 1 of 2 length bytes
    EXPECT_FALSE(WebSocketFramer::is_complete(reinterpret_cast<char*>(data), 3));

    unsigned char data2[4] = {0x81, 0x7E, 0x00, 0x05};  // Length 5
    EXPECT_FALSE(WebSocketFramer::is_complete(reinterpret_cast<char*>(data2), 4));  // Need 5 more bytes
}

TEST(WebSocketFramerTest, IsCompleteWithMaskedFrame) {
    // Masked frame: 0x81 0x85 (mask bit set, len=5) + 4-byte mask + payload
    unsigned char data[12] = {0x81, 0x85, 0x00, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o'};
    EXPECT_TRUE(WebSocketFramer::is_complete(reinterpret_cast<char*>(data), 11));
}

TEST(WebSocketFramerTest, UnmaskPayload) {
    // Simple unmask test
    char masked[] = {'a' ^ 0x00, 'b' ^ 0x01, 'c' ^ 0x02, 'd' ^ 0x03};  // masked with key 0x00,0x01,0x02,0x03
    char unmasked[4];
    char mask_key[] = {0x00, 0x01, 0x02, 0x03};

    bool result = WebSocketFramer::unmask_payload(masked, 4, unmasked, 4, mask_key);
    EXPECT_TRUE(result);
    EXPECT_EQ(unmasked[0], 'a');
    EXPECT_EQ(unmasked[1], 'b');
    EXPECT_EQ(unmasked[2], 'c');
    EXPECT_EQ(unmasked[3], 'd');
}

TEST(WebSocketFramerTest, UnmaskPayloadSizeMismatch) {
    char masked[] = {'a', 'b'};
    char unmasked[4];
    char mask_key[] = {0x00, 0x01, 0x02, 0x03};

    bool result = WebSocketFramer::unmask_payload(masked, 2, unmasked, 4, mask_key);
    EXPECT_FALSE(result);
}

// ============ WebSocketTransport Tests ============

TEST(WebSocketTransportTest, CreateTransport) {
    WebSocketConfig config;
    config.host = "localhost";
    config.port = 8080;
    config.path = "/mcp";

    WebSocketTransport transport(config);
    EXPECT_FALSE(transport.is_connected());
}

TEST(WebSocketTransportTest, CreateTransportDefaultConfig) {
    WebSocketTransport transport;
    EXPECT_FALSE(transport.is_connected());
}

TEST(WebSocketTransportTest, SetUrl) {
    WebSocketTransport transport;
    transport.set_url("ws://example.com:9000/path");

    EXPECT_EQ(transport.url(), "ws://example.com:9000/path");
}

TEST(WebSocketTransportTest, SetUrlWss) {
    WebSocketTransport transport;
    transport.set_url("wss://secure.example.com:443/ws");

    EXPECT_EQ(transport.url(), "wss://secure.example.com:443/ws");
}

TEST(WebSocketTransportTest, ServerModeConfig) {
    WebSocketConfig config;
    config.is_server = true;
    config.port = 8080;
    config.host = "0.0.0.0";

    WebSocketTransport transport(config);
    EXPECT_FALSE(transport.is_connected());
}

TEST(WebSocketTransportTest, SetMessageHandler) {
    WebSocketTransport transport;
    bool called = false;

    transport.on_message([&called](const std::string& msg) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(WebSocketTransportTest, SetErrorHandler) {
    WebSocketTransport transport;
    bool called = false;

    transport.on_error([&called](const std::string& err) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(WebSocketTransportTest, StopWithoutStart) {
    WebSocketTransport transport;
    // Should not crash
    transport.stop();
    EXPECT_FALSE(transport.is_connected());
}

TEST(WebSocketTransportTest, SendWithoutConnection) {
    WebSocketTransport transport;
    bool result = transport.send("test message");
    EXPECT_FALSE(result);
}

// ============ HttpFramer Tests ============

TEST(HttpFramerTest, FrameRequest) {
    std::string body = R"({"jsonrpc":"2.0","method":"test"})";
    std::string framed = HttpFramer::frame(body);

    EXPECT_NE(framed.find("Content-Type: application/json"), std::string::npos);
    EXPECT_NE(framed.find("Content-Length: "), std::string::npos);
    EXPECT_NE(framed.find(body), std::string::npos);
}

TEST(HttpFramerTest, FrameSseEvent) {
    std::string event = "message";
    std::string data = R"({"jsonrpc":"2.0"})";

    std::string framed = HttpFramer::frame_sse(event, data);

    EXPECT_NE(framed.find("event: message"), std::string::npos);
    EXPECT_NE(framed.find("data: "), std::string::npos);
    EXPECT_NE(framed.find(data), std::string::npos);
    EXPECT_NE(framed.find("\r\n\r\n"), std::string::npos);
}

TEST(HttpFramerTest, FrameSseEventNoEvent) {
    std::string data = "test data";
    std::string framed = HttpFramer::frame_sse("", data);

    EXPECT_EQ(framed.find("event:"), std::string::npos);
    EXPECT_NE(framed.find("data: test data"), std::string::npos);
}

TEST(HttpFramerTest, ExtractBodyFromHttpMessage) {
    std::string body = R"({"test":"data"})";
    std::string http_msg =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;

    std::string extracted = HttpFramer::extract_body(http_msg);
    EXPECT_EQ(extracted, body);
}

TEST(HttpFramerTest, ExtractBodyInvalid) {
    std::string http_msg = "Invalid HTTP message without proper headers";
    std::string extracted = HttpFramer::extract_body(http_msg);
    EXPECT_EQ(extracted, "");
}

TEST(HttpFramerTest, ParseContentLength) {
    std::string headers =
        "Content-Type: text/plain\r\n"
        "Content-Length: 1234\r\n"
        "Cache-Control: no-cache";

    int length = HttpFramer::parse_content_length(headers);
    EXPECT_EQ(length, 1234);
}

TEST(HttpFramerTest, ParseContentLengthNotFound) {
    std::string headers =
        "Content-Type: text/plain\r\n"
        "Cache-Control: no-cache";

    int length = HttpFramer::parse_content_length(headers);
    EXPECT_EQ(length, -1);
}

TEST(HttpFramerTest, ParseContentLengthCaseInsensitive) {
    std::string headers = "content-length: 5678\r\n";

    int length = HttpFramer::parse_content_length(headers);
    EXPECT_EQ(length, 5678);
}

// ============ HttpTransport Tests ============

TEST(HttpTransportTest, CreateTransportAsServer) {
    HttpTransport transport(8080);
    EXPECT_FALSE(transport.is_connected());
}

TEST(HttpTransportTest, CreateTransportAsClient) {
    HttpTransport transport("localhost", 8080);
    EXPECT_FALSE(transport.is_connected());
}

TEST(HttpTransportTest, SetEndpoint) {
    HttpTransport transport(8080);
    transport.set_endpoint("/api/mcp");
    // Just verify it doesn't crash
}

TEST(HttpTransportTest, SetMessageHandler) {
    HttpTransport transport(8080);
    bool called = false;

    transport.on_message([&called](const std::string& msg) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(HttpTransportTest, SetErrorHandler) {
    HttpTransport transport(8080);
    bool called = false;

    transport.on_error([&called](const std::string& err) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(HttpTransportTest, StopWithoutStart) {
    HttpTransport transport(8080);
    transport.stop();
    EXPECT_FALSE(transport.is_connected());
}

// ============ StdioFramer Tests ============

TEST(StdioFramerTest, FrameMessage) {
    std::string message = "test message";
    std::string framed = StdioFramer::frame(message);

    EXPECT_EQ(framed, message + '\n');
}

TEST(StdioFramerTest, FrameEmptyMessage) {
    std::string message = "";
    std::string framed = StdioFramer::frame(message);

    EXPECT_EQ(framed, "\n");
}

TEST(StdioFramerTest, DecodeSingleMessage) {
    std::string data = "hello\nworld";
    auto messages = StdioFramer::decode(data.c_str(), data.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0], "hello");
}

TEST(StdioFramerTest, DecodeMultipleMessages) {
    std::string data = "first\nsecond\nthird\n";
    auto messages = StdioFramer::decode(data.c_str(), data.size());

    ASSERT_EQ(messages.size(), 3);
    EXPECT_EQ(messages[0], "first");
    EXPECT_EQ(messages[1], "second");
    EXPECT_EQ(messages[2], "third");
}

TEST(StdioFramerTest, DecodeWithRemainder) {
    std::string data = "complete\nincomplet";
    std::string remainder;

    auto messages = StdioFramer::decode(data.c_str(), data.size(), remainder);

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0], "complete");
    EXPECT_EQ(remainder, "incomplet");
}

TEST(StdioFramerTest, DecodeEmptyInput) {
    std::string data = "";
    auto messages = StdioFramer::decode(data.c_str(), data.size());

    EXPECT_TRUE(messages.empty());
}

TEST(StdioFramerTest, DecodeNoDelimiter) {
    std::string data = "no delimiter here";
    auto messages = StdioFramer::decode(data.c_str(), data.size());

    EXPECT_TRUE(messages.empty());
}

// ============ StdioTransport Tests ============

TEST(StdioTransportTest, CreateTransport) {
    StdioTransport transport;
    EXPECT_FALSE(transport.is_connected());
}

TEST(StdioTransportTest, SetReadFromStdin) {
    StdioTransport transport;
    transport.set_read_from_stdin(false);
    // Just verify it doesn't crash
    transport.set_read_from_stdin(true);
}

TEST(StdioTransportTest, SetMessageHandler) {
    StdioTransport transport;
    bool called = false;

    transport.on_message([&called](const std::string& msg) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(StdioTransportTest, SetErrorHandler) {
    StdioTransport transport;
    bool called = false;

    transport.on_error([&called](const std::string& err) {
        called = true;
    });

    EXPECT_FALSE(called);
}

TEST(StdioTransportTest, StopWithoutStart) {
    StdioTransport transport;
    transport.stop();
    EXPECT_FALSE(transport.is_connected());
}

TEST(StdioTransportTest, SendWithoutConnection) {
    StdioTransport transport;
    // StdioTransport::send() writes to stdout, which is always available
    // So it returns true even without start()
    bool result = transport.send("test message");
    EXPECT_TRUE(result);
}

// ============ TransportFactory Tests ============

TEST(TransportFactoryTest, CreateStdioTransport) {
    auto transport = TransportFactory::create(TransportFactory::Type::Stdio);
    EXPECT_NE(transport, nullptr);
    EXPECT_FALSE(transport->is_connected());
}

TEST(TransportFactoryTest, CreateHttpTransport) {
    auto transport = TransportFactory::create(TransportFactory::Type::Http);
    EXPECT_NE(transport, nullptr);
    EXPECT_FALSE(transport->is_connected());
}

TEST(TransportFactoryTest, CreateWebSocketTransport) {
    auto transport = TransportFactory::create(TransportFactory::Type::WebSocket);
    EXPECT_NE(transport, nullptr);
    EXPECT_FALSE(transport->is_connected());
}

TEST(TransportFactoryTest, CreateByStringStdio) {
    auto transport = TransportFactory::create("stdio");
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactoryTest, CreateByStringHttp) {
    auto transport = TransportFactory::create("http");
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactoryTest, CreateByStringSse) {
    auto transport = TransportFactory::create("sse");
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactoryTest, CreateByStringWebSocket) {
    auto transport = TransportFactory::create("websocket");
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactoryTest, CreateByStringWs) {
    auto transport = TransportFactory::create("ws");
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactoryTest, CreateInvalidType) {
    auto transport = TransportFactory::create("invalid");
    EXPECT_EQ(transport, nullptr);
}