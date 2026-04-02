/**
 * @file stdio_client.cpp
 * @brief MCP stdio client for testing stdio_server
 */

#include "mcpp/core/json.hpp"
#include "mcpp/protocol/message.hpp"
#include "mcpp/transport/transport.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <map>

#define LOG(msg) std::cerr << "[CLIENT] " << msg << std::endl

using namespace mcpp;

class StdioClient {
public:
    StdioClient() : request_id_(1) {}

    ~StdioClient() {
        if (transport_) {
            transport_->stop();
        }
    }

    bool connect() {
        transport_ = TransportFactory::create(TransportFactory::Type::Stdio);
        if (!transport_) {
            LOG("Failed to create transport");
            return false;
        }

        transport_->on_message([this](const std::string& msg) {
            handle_response(msg);
        });

        transport_->on_error([](const std::string& err) {
            LOG("Transport error: " + err);
        });

        if (!transport_->start()) {
            LOG("Failed to start transport");
            return false;
        }

        return true;
    }

    bool initialize() {
        JsonValue params = JsonValue::object();
        params["protocolVersion"] = "2025-06-18";
        params["capabilities"] = JsonValue::object();

        JsonValue response = send_request_sync("initialize", params);
        
        if (response.is_null() || response.contains("error")) {
            return false;
        }

        if (response.contains("result")) {
            send_notification("initialized", JsonValue::object());
            return true;
        }
        return false;
    }

    JsonValue list_tools() {
        return send_request_sync("tools/list", JsonValue::object());
    }

    JsonValue call_tool(const std::string& name, const JsonValue& args) {
        JsonValue params = JsonValue::object();
        params["name"] = name;
        params["arguments"] = args;
        return send_request_sync("tools/call", params);
    }

    JsonValue list_resources() {
        return send_request_sync("resources/list", JsonValue::object());
    }

    JsonValue read_resource(const std::string& uri) {
        JsonValue params = JsonValue::object();
        params["uri"] = uri;
        return send_request_sync("resources/read", params);
    }

    JsonValue list_prompts() {
        return send_request_sync("prompts/list", JsonValue::object());
    }

    JsonValue get_prompt(const std::string& name, const JsonValue& args) {
        JsonValue params = JsonValue::object();
        params["name"] = name;
        params["arguments"] = args;
        return send_request_sync("prompts/get", params);
    }

private:
    JsonValue send_request_sync(const std::string& method, const JsonValue& params) {
        int id = request_id_++;

        JsonRpcRequest request;
        request.jsonrpc = "2.0";
        request.method = method;
        request.params = params;
        request.id = id;

        std::string json_str = MessageSerializer::serialize(request);
        
        pending_id_ = id;
        last_response_ = JsonValue();

        if (!transport_->send(json_str)) {
            return JsonValue();
        }

        // Wait for response with timeout
        int retries = 50;
        while (retries-- > 0) {
            if (!last_response_.is_null()) {
                return last_response_;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return JsonValue();
    }

    void send_notification(const std::string& method, const JsonValue& params) {
        JsonRpcRequest notification;
        notification.jsonrpc = "2.0";
        notification.method = method;
        notification.params = params;
        std::string json_str = MessageSerializer::serialize(notification);
        transport_->send(json_str);
    }

    void handle_response(const std::string& message) {
        auto result = MessageParser::parse(message);
        if (!result.ok()) return;

        const auto& msg = result.value();
        if (msg.type() == MessageType::Response) {
            const auto& resp = msg.response;
            
            JsonValue response_obj = JsonValue::object();
            if (resp.is_error) {
                response_obj["error"] = resp.error;
            } else {
                response_obj["result"] = resp.result;
            }
            
            last_response_ = response_obj;
        }
    }

    std::unique_ptr<ITransport> transport_;
    int request_id_;
    int pending_id_ = -1;
    JsonValue last_response_;
};

int main(int argc, char* argv[]) {
    LOG("MCP Stdio Client Starting...");

    StdioClient client;
    
    if (!client.connect()) {
        LOG("Failed to connect");
        return 1;
    }

    if (!client.initialize()) {
        LOG("Failed to initialize");
        return 1;
    }
    LOG("Initialized successfully");

    // Test tools/list
    LOG("Testing tools/list...");
    JsonValue tools_result = client.list_tools();
    if (tools_result.contains("result")) {
        LOG("Tools listed successfully");
    }

    // Test tools/call - echo
    LOG("Testing tools/call - echo...");
    JsonValue echo_args = JsonValue::object();
    echo_args["message"] = "Hello from client";
    JsonValue echo_result = client.call_tool("echo", echo_args);
    if (echo_result.contains("result")) {
        LOG("Echo tool works");
    }

    // Test resources/list
    LOG("Testing resources/list...");
    JsonValue resources_result = client.list_resources();
    if (resources_result.contains("result")) {
        LOG("Resources listed successfully");
    }

    // Test resources/read
    LOG("Testing resources/read...");
    JsonValue resource_result = client.read_resource("config://server");
    if (resource_result.contains("result")) {
        LOG("Resource read successfully");
    }

    // Test prompts/list
    LOG("Testing prompts/list...");
    JsonValue prompts_result = client.list_prompts();
    if (prompts_result.contains("result")) {
        LOG("Prompts listed successfully");
    }

    // Test prompts/get
    LOG("Testing prompts/get...");
    JsonValue prompt_args = JsonValue::object();
    prompt_args["code"] = "int main() {}";
    JsonValue prompt_result = client.get_prompt("code_review", prompt_args);
    if (prompt_result.contains("result")) {
        LOG("Prompt retrieved successfully");
    }

    LOG("All tests completed!");
    return 0;
}
