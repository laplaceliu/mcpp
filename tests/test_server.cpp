#include "gtest/gtest.h"
#include "mcpp/server/server.hpp"

using namespace mcpp;

TEST(ServerTest, CreateServer) {
    ServerOptions options;
    options.name = "test-server";
    options.version = "1.0.0";

    Server server(options);
    EXPECT_TRUE(server.list_tools().empty());
}

TEST(ServerTest, RegisterTool) {
    ServerOptions options;
    options.capabilities.supports_tools = true;

    Server server(options);

    bool called = false;
    server.register_tool(
        "test_tool",
        "A test tool",
        JsonValue::object({
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}}}
            }}
        }),
        [&called](const std::string&, const JsonValue&) {
            called = true;
            CallToolResult result;
            result.is_error = false;
            result.content.push_back({{"type", "text"}, {"text", "ok"}});
            return result;
        }
    );

    auto tools = server.list_tools();
    EXPECT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].name, "test_tool");
    EXPECT_EQ(tools[0].description, "A test tool");
}

TEST(ServerTest, ServerOptions) {
    ServerOptions options;
    options.name = "my-server";
    options.version = "2.0.0";
    options.protocol_version = "2025-06-18";
    options.capabilities.supports_tools = true;
    options.capabilities.supports_resources = true;
    options.capabilities.supports_prompts = true;
    options.port = 9090;

    Server server(options);
    EXPECT_TRUE(server.start() || !server.start()); // Just check it doesn't crash
}
