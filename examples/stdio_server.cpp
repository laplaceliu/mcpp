/**
 * @file stdio_server.cpp
 * @brief Example MCP stdio server
 */
#include "mcpp/server/server.hpp"
#include <iostream>

using namespace mcpp;

// Example: Echo tool
CallToolResult echo_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;
    std::string input = args.value("message", std::string());
    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", "Echo: " + input}
    }));
    return result;
}

// Example: Add tool
CallToolResult add_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;
    int a = args.value("a", 0);
    int b = args.value("b", 0);
    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", std::to_string(a + b)}
    }));
    return result;
}

int main() {
    // Create server options
    ServerOptions options;
    options.name = "example-server";
    options.version = "1.0.0";
    options.capabilities.supports_tools = true;
    options.capabilities.supports_resources = true;

    // Create server
    auto server = std::make_shared<Server>(options);

    // Register tools
    server->register_tool(
        "echo",
        "Echo back the input message",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"message", JsonValue::object({
                    {"type", "string"},
                    {"description", "Message to echo"}
                })}
            })},
            {"required", JsonValue::array({"message"})}
        }),
        echo_tool
    );

    server->register_tool(
        "add",
        "Add two numbers",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"a", JsonValue::object({
                    {"type", "number"},
                    {"description", "First number"}
                })},
                {"b", JsonValue::object({
                    {"type", "number"},
                    {"description", "Second number"}
                })}
            })},
            {"required", JsonValue::array({"a", "b"})}
        }),
        add_tool
    );

    // Start server
    if (!server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started, waiting for requests..." << std::endl;

    // Keep running
    server->wait();

    return 0;
}
