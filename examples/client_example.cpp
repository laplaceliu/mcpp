/**
 * @file client_example.cpp
 * @brief Example MCP Client demonstrating client functionality
 */

#include <iostream>
#include <memory>
#include "mcpp/client/client.hpp"
#include "mcpp/core/json.hpp"

using namespace mcpp;

int main() {
    std::cout << "MCPP Client Example" << std::endl;
    std::cout << "==================" << std::endl;

    // Configure client options
    ClientOptions options;
    options.name = "example-client";
    options.version = "1.0.0";
    options.protocol_version = "2025-06-18";

    // Enable client capabilities
    options.capabilities.supports_sampling = true;
    options.capabilities.supports_elicitation = true;
    options.capabilities.supports_logging = true;

    // Use stdio transport (for subprocess communication)
    options.transport_type = TransportFactory::Type::Stdio;

    // Create and start client
    auto client = std::make_shared<Client>(options);

    // Set up sampling handler (server requests LLM completion)
    client->on_sampling_request([](const SamplingParams& params) {
        std::cout << "\n[Client] Received sampling request:" << std::endl;
        std::cout << "  Method: " << params.method << std::endl;
        std::cout << "  System prompt: " << params.system_prompt << std::endl;
        std::cout << "  Messages: " << params.messages.size() << std::endl;

        SamplingResult result;
        result.is_error = false;

        // Create a sample response
        result.content = JsonValue::object({
            {"role", "assistant"},
            {"content", JsonValue::array({
                JsonValue::object({
                    {"type", "text"},
                    {"text", "This is a sample LLM completion from the client."}
                })
            })}
        });

        return result;
    });

    // Set up elicitation handler (server requests user input)
    client->on_elicitation_request([](const ElicitationParams& params) {
        std::cout << "\n[Client] Received elicitation request:" << std::endl;
        std::cout << "  Message: " << params.message << std::endl;
        std::cout << "  Options: " << params.options.size() << std::endl;

        ElicitationResult result;
        result.is_error = false;

        // Return a sample response
        result.content = JsonValue::object({
            {"type", "text"},
            {"text", "User selected option"}
        });

        return result;
    });

    // Set up progress notification handler
    client->on_progress_notification([](double progress, double total, const std::string& message) {
        std::cout << "\n[Client] Progress: " << (progress / total * 100) << "% - " << message << std::endl;
    });

    // Start the client
    if (!client->start()) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "\nClient started successfully" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    // Send a logging message (demonstration)
    client->logging_message("info", JsonValue::object({
        {"source", "example-client"},
        {"event", "client_started"}
    }));

    // Example: Show how a server could request sampling
    // (In real usage, this would be triggered by server requests)
    SamplingParams sampling_params;
    sampling_params.method = "text/completion";
    sampling_params.system_prompt = "You are a helpful assistant.";
    sampling_params.messages.push_back(SamplingMessage{
        "user",
        {content::make_text("Hello, how are you?")}
    });

    std::cout << "\nDemonstrating sampling request..." << std::endl;
    client->sampling_complete(sampling_params, [](const SamplingResult& result) {
        if (result.is_error) {
            std::cout << "  Sampling error: " << result.error << std::endl;
        } else {
            std::cout << "  Sampling result received" << std::endl;
        }
    });

    // Wait for messages (in real usage, this would block)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Clean shutdown
    client->stop();
    std::cout << "\nClient stopped" << std::endl;

    return 0;
}