/**
 * @file client.hpp
 * @brief MCP Client implementation
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>
#include "mcpp/core/types.hpp"
#include "mcpp/protocol/message.hpp"
#include "mcpp/transport/transport.hpp"

namespace mcpp {

/**
 * @brief Client options
 */
struct ClientOptions {
    std::string name = "mcpp-client";          ///< Client name
    std::string version = "1.0.0";             ///< Client version
    std::string protocol_version = "2025-06-18"; ///< MCP protocol version

    ClientCapabilities capabilities;            ///< Client capabilities

    TransportFactory::Type transport_type = TransportFactory::Type::Stdio; ///< Transport type
    std::string host = "localhost";             ///< Server host (for HTTP)
    int port = 8080;                           ///< Server port (for HTTP)
};

/**
 * @brief Result of sampling request
 */
struct SamplingResult {
    bool is_error = false;                     ///< Whether the request resulted in error
    JsonValue content;                         ///< Generated content
    std::string error;                         ///< Error message if is_error is true
};

/**
 * @brief Result of elicitation request
 */
struct ElicitationResult {
    bool is_error = false;                     ///< Whether the request resulted in error
    JsonValue content;                         ///< User's response content
    std::string error;                         ///< Error message if is_error is true
};

/**
 * @brief Message content for prompts
 */
struct ContentBlock {
    std::string type;                          ///< Content type (text, image, audio)
    std::string text;                          ///< Text content (if type is "text")
    std::string mime_type;                     ///< MIME type for binary content
    std::string data;                          ///< Base64 encoded binary data
};

/**
 * @brief Sampling message for LLM interaction
 */
struct SamplingMessage {
    std::string role;                          ///< "user" or "assistant"
    std::vector<ContentBlock> content;         ///< Message content
};

/**
 * @brief Parameters for sampling request
 */
struct SamplingParams {
    std::string method;                        ///< Sampling method (e.g., "text/completion")
    std::vector<SamplingMessage> messages;    ///< Conversation messages
    std::string system_prompt;                 ///< System prompt
    JsonValue params;                          ///< Additional parameters
};

/**
 * @brief Parameters for elicitation request
 */
struct ElicitationParams {
    std::string message;                       ///< Message to present to user
    std::vector<ContentBlock> prefill;         ///< Prefill content
    std::vector<JsonValue> options;            ///< Response options
    bool requested_schema_is_partial = false; ///< Whether schema is partial
    JsonValue requested_schema;                ///< Schema for expected response
};

/**
 * @brief Main MCP Client class
 * @details Implements the Model Context Protocol client-side functionality
 * @note This class allows a server to request sampling, elicitation, and logging
 */
class Client : public std::enable_shared_from_this<Client> {
public:
    /**
     * @brief Construct a client
     * @param options Client configuration options
     */
    explicit Client(const ClientOptions& options = ClientOptions())
        : options_(options), running_(false) {}

    /**
     * @brief Destructor - stops the client
     */
    ~Client() {
        stop();
    }

    // ============ Lifecycle ============

    /**
     * @brief Start the client
     * @return true if started successfully
     */
    bool start() {
        if (running_) {
            return false;
        }

        // Create transport based on type
        transport_ = TransportFactory::create(options_.transport_type);
        if (!transport_) {
            std::cerr << "Failed to create transport" << std::endl;
            return false;
        }

        // Set up message handler
        transport_->on_message([this](const std::string& msg) {
            handle_message(msg);
        });

        // Set up error handler
        transport_->on_error([](const std::string& err) {
            std::cerr << "Transport error: " << err << std::endl;
        });

        // Start transport
        if (!transport_->start()) {
            std::cerr << "Failed to start transport" << std::endl;
            return false;
        }

        running_ = true;
        return true;
    }

    /**
     * @brief Stop the client
     */
    void stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        if (transport_) {
            transport_->stop();
        }

        if (client_thread_.joinable()) {
            client_thread_.join();
        }
    }

    /**
     * @brief Wait for client thread to complete
     */
    void wait() {
        if (client_thread_.joinable()) {
            client_thread_.join();
        }
    }

    // ============ Sampling (Server -> Client) ============

    /**
     * @brief Send a sampling request to the client
     * @param params Sampling parameters
     * @param callback Callback with the result
     * @note This is called by the server to request LLM completion
     */
    void sampling_complete(const SamplingParams& params,
                          std::function<void(const SamplingResult&)> callback) {
        JsonValue json_params = JsonValue::object();
        json_params["method"] = params.method;
        json_params["messages"] = JsonValue::array();
        for (const auto& msg : params.messages) {
            JsonValue msg_obj = JsonValue::object();
            msg_obj["role"] = msg.role;
            msg_obj["content"] = JsonValue::array();
            for (const auto& block : msg.content) {
                JsonValue block_obj = JsonValue::object();
                block_obj["type"] = block.type;
                if (!block.text.empty()) {
                    block_obj["text"] = block.text;
                }
                if (!block.mime_type.empty()) {
                    block_obj["mimeType"] = block.mime_type;
                }
                if (!block.data.empty()) {
                    block_obj["data"] = block.data;
                }
                msg_obj["content"].push_back(block_obj);
            }
            json_params["messages"].push_back(msg_obj);
        }
        if (!params.system_prompt.empty()) {
            json_params["systemPrompt"] = params.system_prompt;
        }
        if (!params.params.is_null()) {
            json_params["params"] = params.params;
        }

        send_request("sampling/complete", json_params, [callback](const JsonRpcResponse& resp) {
            SamplingResult result;
            if (resp.is_error) {
                result.is_error = true;
                if (resp.error.contains("message")) {
                    result.error = resp.error["message"].get<std::string>();
                }
            } else {
                result.content = resp.result;
            }
            callback(result);
        });
    }

    // ============ Elicitation (Server -> Client) ============

    /**
     * @brief Send an elicitation request to the client
     * @param params Elicitation parameters
     * @param callback Callback with the result
     * @note This is called by the server to request user input
     */
    void elicitation_request(const ElicitationParams& params,
                            std::function<void(const ElicitationResult&)> callback) {
        JsonValue json_params = JsonValue::object();
        json_params["message"] = params.message;
        json_params["prefill"] = JsonValue::array();
        for (const auto& block : params.prefill) {
            JsonValue block_obj = JsonValue::object();
            block_obj["type"] = block.type;
            if (!block.text.empty()) {
                block_obj["text"] = block.text;
            }
            json_params["prefill"].push_back(block_obj);
        }
        json_params["options"] = JsonValue::array();
        for (const auto& opt : params.options) {
            json_params["options"].push_back(opt);
        }
        json_params["requestedSchemaIsPartial"] = params.requested_schema_is_partial;
        json_params["requestedSchema"] = params.requested_schema;

        send_request("elicitation/request", json_params, [callback](const JsonRpcResponse& resp) {
            ElicitationResult result;
            if (resp.is_error) {
                result.is_error = true;
                if (resp.error.contains("message")) {
                    result.error = resp.error["message"].get<std::string>();
                }
            } else {
                result.content = resp.result;
            }
            callback(result);
        });
    }

    // ============ Logging (Server -> Client) ============

    /**
     * @brief Send a log message to the client
     * @param level Log level (debug, info, warning, error)
     * @param data Log data
     * @note This is called by the server to send log messages
     */
    void logging_message(const std::string& level, const JsonValue& data) {
        JsonValue params = JsonValue::object();
        params["level"] = level;
        params["data"] = data;

        // Logging is a notification, no response expected
        std::string json_str = MessageSerializer::serialize(
            JsonRpcRequest{"2.0", "logging/message", JsonValue(), params});
        transport_->send(json_str);
    }

    // ============ Progress (Server -> Client) ============

    /**
     * @brief Send a progress notification to the client
     * @param progress Progress value (0.0 to 1.0)
     * @param total Total value
     * @param message Progress message
     */
    void progress_report(double progress, double total, const std::string& message) {
        JsonValue params = JsonValue::object();
        params["progress"] = progress;
        params["total"] = total;
        params["message"] = message;

        // Progress is a notification, no response expected
        std::string json_str = MessageSerializer::serialize(
            JsonRpcRequest{"2.0", "notifications/progress", JsonValue(), params});
        transport_->send(json_str);
    }

    // ============ Client Callbacks (Set by Server) ============

    /**
     * @brief Set callback for sampling request received from server
     * @param handler Callback function
     */
    void on_sampling_request(std::function<SamplingResult(const SamplingParams&)> handler) {
        sampling_handler_ = handler;
    }

    /**
     * @brief Set callback for elicitation request received from server
     * @param handler Callback function
     */
    void on_elicitation_request(std::function<ElicitationResult(const ElicitationParams&)> handler) {
        elicitation_handler_ = handler;
    }

    /**
     * @brief Set callback for progress notification from server
     * @param handler Callback function
     */
    void on_progress_notification(std::function<void(double, double, const std::string&)> handler) {
        progress_handler_ = handler;
    }

    // ============ Connection State ============

    /**
     * @brief Check if client is connected
     * @return true if connected
     */
    bool is_connected() const {
        return running_ && transport_ && transport_->is_connected();
    }

    /**
     * @brief Get client capabilities
     * @return Client capabilities
     */
    const ClientCapabilities& capabilities() const { return options_.capabilities; }

private:
    void setup_handlers() {
        // Set up internal handlers for responses
    }

    void handle_message(const std::string& message) {
        auto result = MessageParser::parse(message);
        if (!result.ok()) {
            std::cerr << "Failed to parse message: " << result.error() << std::endl;
            return;
        }

        const auto& msg = result.value();
        if (msg.type() == MessageType::Request || msg.type() == MessageType::Notification) {
            handle_notification(msg.request);
        } else if (msg.type() == MessageType::Response) {
            // Handle response (for callbacks)
        }
    }

    void handle_notification(const JsonRpcRequest& notification) {
        const std::string& method = notification.method;

        if (method == "sampling/complete") {
            if (sampling_handler_) {
                SamplingParams params;
                if (!notification.params.is_null()) {
                    if (notification.params.contains("method")) {
                        params.method = notification.params["method"].get<std::string>();
                    }
                    if (notification.params.contains("messages")) {
                        for (const auto& msg_json : notification.params["messages"]) {
                            SamplingMessage msg;
                            msg.role = msg_json["role"].get<std::string>();
                            if (msg_json.contains("content")) {
                                for (const auto& block_json : msg_json["content"]) {
                                    ContentBlock block;
                                    block.type = block_json["type"].get<std::string>();
                                    if (block_json.contains("text")) {
                                        block.text = block_json["text"].get<std::string>();
                                    }
                                    if (block_json.contains("mimeType")) {
                                        block.mime_type = block_json["mimeType"].get<std::string>();
                                    }
                                    if (block_json.contains("data")) {
                                        block.data = block_json["data"].get<std::string>();
                                    }
                                    msg.content.push_back(block);
                                }
                            }
                            if (msg_json.contains("systemPrompt")) {
                                params.system_prompt = msg_json["systemPrompt"].get<std::string>();
                            }
                            params.messages.push_back(msg);
                        }
                    }
                    if (notification.params.contains("params")) {
                        params.params = notification.params["params"];
                    }
                }

                SamplingResult result = sampling_handler_(params);

                // Send response
                JsonRpcResponse resp;
                resp.id = notification.id;
                if (result.is_error) {
                    resp.is_error = true;
                    resp.error = JsonValue::object({
                        {"code", -32600},
                        {"message", result.error}
                    });
                } else {
                    resp.result = result.content;
                }
                transport_->send(MessageSerializer::serialize(resp));
            }
        } else if (method == "elicitation/request") {
            if (elicitation_handler_) {
                ElicitationParams params;
                if (!notification.params.is_null()) {
                    if (notification.params.contains("message")) {
                        params.message = notification.params["message"].get<std::string>();
                    }
                    if (notification.params.contains("prefill")) {
                        for (const auto& block_json : notification.params["prefill"]) {
                            ContentBlock block;
                            block.type = block_json["type"].get<std::string>();
                            if (block_json.contains("text")) {
                                block.text = block_json["text"].get<std::string>();
                            }
                            params.prefill.push_back(block);
                        }
                    }
                    if (notification.params.contains("options")) {
                        for (const auto& opt : notification.params["options"]) {
                            params.options.push_back(opt);
                        }
                    }
                    if (notification.params.contains("requestedSchemaIsPartial")) {
                        params.requested_schema_is_partial =
                            notification.params["requestedSchemaIsPartial"].get<bool>();
                    }
                    if (notification.params.contains("requestedSchema")) {
                        params.requested_schema = notification.params["requestedSchema"];
                    }
                }

                ElicitationResult result = elicitation_handler_(params);

                // Send response
                JsonRpcResponse resp;
                resp.id = notification.id;
                if (result.is_error) {
                    resp.is_error = true;
                    resp.error = JsonValue::object({
                        {"code", -32600},
                        {"message", result.error}
                    });
                } else {
                    resp.result = result.content;
                }
                transport_->send(MessageSerializer::serialize(resp));
            }
        } else if (method == "notifications/progress") {
            if (progress_handler_ && !notification.params.is_null()) {
                double progress = 0.0;
                double total = 0.0;
                std::string message;

                if (notification.params.contains("progress")) {
                    progress = notification.params["progress"].get<double>();
                }
                if (notification.params.contains("total")) {
                    total = notification.params["total"].get<double>();
                }
                if (notification.params.contains("message")) {
                    message = notification.params["message"].get<std::string>();
                }

                progress_handler_(progress, total, message);
            }
        }
    }

    void send_request(const std::string& method, const JsonValue& params,
                     std::function<void(const JsonRpcResponse&)> callback) {
        static int request_id = 1;
        int id = request_id++;

        JsonRpcRequest request;
        request.jsonrpc = "2.0";
        request.method = method;
        request.params = params;
        request.id = id;

        std::string json_str = MessageSerializer::serialize(request);
        if (transport_->send(json_str)) {
            // Callback would be handled when response arrives
            // For now, this is a simplified implementation
            (void)callback;
        }
    }

    ClientOptions options_;
    std::unique_ptr<ITransport> transport_;

    std::thread client_thread_;
    std::atomic<bool> running_;

    // Handlers set by server
    std::function<SamplingResult(const SamplingParams&)> sampling_handler_;
    std::function<ElicitationResult(const ElicitationParams&)> elicitation_handler_;
    std::function<void(double, double, const std::string&)> progress_handler_;
};

/**
 * @brief Helper utilities for creating content blocks
 */
namespace content {

/**
 * @brief Create a text content block
 * @param text Text content
 * @return ContentBlock for text
 */
inline ContentBlock make_text(const std::string& text) {
    ContentBlock block;
    block.type = "text";
    block.text = text;
    return block;
}

/**
 * @brief Create an image content block
 * @param data Base64 encoded image data
 * @param mime_type Image MIME type
 * @return ContentBlock for image
 */
inline ContentBlock make_image(const std::string& data, const std::string& mime_type) {
    ContentBlock block;
    block.type = "image";
    block.data = data;
    block.mime_type = mime_type;
    return block;
}

} // namespace content

} // namespace mcpp