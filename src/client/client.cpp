/**
 * @file client.cpp
 * @brief MCP Client implementation
 */

#include "mcpp/client/client.hpp"
#include "mcpp/transport/transport.hpp"
#include <iostream>

namespace mcpp {

Client::Client(const ClientOptions& options)
    : options_(options), running_(false) {
}

Client::~Client() {
    stop();
}

bool Client::start() {
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

void Client::stop() {
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

void Client::wait() {
    if (client_thread_.joinable()) {
        client_thread_.join();
    }
}

void Client::sampling_complete(const SamplingParams& params,
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

void Client::elicitation_request(const ElicitationParams& params,
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

void Client::logging_message(const std::string& level, const JsonValue& data) {
    JsonValue params = JsonValue::object();
    params["level"] = level;
    params["data"] = data;

    // Logging is a notification, no response expected
    std::string json_str = MessageSerializer::serialize(
        JsonRpcRequest{"2.0", "logging/message", JsonValue(), params});
    transport_->send(json_str);
}

void Client::progress_report(double progress, double total, const std::string& message) {
    JsonValue params = JsonValue::object();
    params["progress"] = progress;
    params["total"] = total;
    params["message"] = message;

    // Progress is a notification, no response expected
    std::string json_str = MessageSerializer::serialize(
        JsonRpcRequest{"2.0", "notifications/progress", JsonValue(), params});
    transport_->send(json_str);
}

void Client::on_sampling_request(std::function<SamplingResult(const SamplingParams&)> handler) {
    sampling_handler_ = handler;
}

void Client::on_elicitation_request(std::function<ElicitationResult(const ElicitationParams&)> handler) {
    elicitation_handler_ = handler;
}

void Client::on_progress_notification(std::function<void(double, double, const std::string&)> handler) {
    progress_handler_ = handler;
}

bool Client::is_connected() const {
    return running_ && transport_ && transport_->is_connected();
}

void Client::setup_handlers() {
    // Set up internal handlers for responses
}

void Client::handle_message(const std::string& message) {
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

void Client::handle_notification(const JsonRpcRequest& notification) {
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

void Client::send_request(const std::string& method, const JsonValue& params,
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

} // namespace mcpp