/**
 * @file message.hpp
 * @brief JSON-RPC message parsing and serialization
 */
#pragma once

#include "mcpp/core/types.hpp"
#include "mcpp/core/error.hpp"

namespace mcpp {

/**
 * @brief Message type enumeration
 */
enum class MessageType {
    Request,      ///< Request message (expects response)
    Response,     ///< Response message
    Notification, ///< Notification (no response expected)
    Error        ///< Invalid or error message
};

/**
 * @brief JSON-RPC message container
 * @details Can hold either a request/notification or a response
 */
class JsonRpcMessage {
public:
    /**
     * @brief Get the message type
     * @return MessageType
     */
    MessageType type() const { return type_; }

    /**
     * @brief Set the message type
     * @param t New message type
     */
    void set_type(MessageType t) { type_ = t; }

    /**
     * @brief Get this message as a request (if applicable)
     * @return Pointer to request, or nullptr if not a request
     */
    const JsonRpcRequest* as_request() const {
        return type_ == MessageType::Request ? &request : nullptr;
    }

    /**
     * @brief Get this message as a response (if applicable)
     * @return Pointer to response, or nullptr if not a response
     */
    const JsonRpcResponse* as_response() const {
        return type_ == MessageType::Response ? &response : nullptr;
    }

    JsonRpcRequest request;    ///< Request/notification data
    JsonRpcResponse response;  ///< Response data

private:
    MessageType type_ = MessageType::Error;
};

/**
 * @brief Parser for JSON-RPC messages
 */
class MessageParser {
public:
    /**
     * @brief Parse a JSON string into a message
     * @param data JSON string to parse
     * @return Result containing parsed message or error
     */
    static Result<JsonRpcMessage> parse(const std::string& data) {
        return parse(data.c_str(), data.size());
    }

    /**
     * @brief Parse a character buffer into a message
     * @param data Pointer to data buffer
     * @param len Length of data
     * @return Result containing parsed message or error
     */
    static Result<JsonRpcMessage> parse(const char* data, size_t len) {
        try {
            JsonValue obj = JsonValue::parse(data, data + len);
            JsonRpcMessage msg;
            msg.set_type(detect_type(obj));

            if (msg.type() == MessageType::Request || msg.type() == MessageType::Notification) {
                msg.request.jsonrpc = obj.value("jsonrpc", std::string("2.0"));
                msg.request.method = obj.value("method", std::string());
                msg.request.id = obj.value("id", JsonValue());
                if (obj.contains("params")) {
                    msg.request.params = obj["params"];
                }
            } else if (msg.type() == MessageType::Response) {
                msg.response.jsonrpc = obj.value("jsonrpc", std::string("2.0"));
                msg.response.id = obj.value("id", JsonValue());
                if (obj.contains("result")) {
                    msg.response.result = obj["result"];
                    msg.response.is_error = false;
                }
                if (obj.contains("error")) {
                    msg.response.error = obj["error"];
                    msg.response.is_error = true;
                }
            } else {
                return Result<JsonRpcMessage>("Unknown message type");
            }

            return Result<JsonRpcMessage>(msg);
        } catch (const std::exception& e) {
            return Result<JsonRpcMessage>(std::string("Parse error: ") + e.what());
        }
    }

private:
    /**
     * @brief Detect message type from JSON object
     * @param obj JSON object to inspect
     * @return Detected message type
     */
    static MessageType detect_type(const JsonValue& obj) {
        if (!obj.is_object()) return MessageType::Error;

        // Check if it's a response (has result or error field)
        if (obj.contains("result") || obj.contains("error")) {
            return MessageType::Response;
        }

        // Check if it's a request (has method field)
        if (obj.contains("method")) {
            // Has id is request, no id is notification
            if (obj.contains("id")) {
                return MessageType::Request;
            } else {
                return MessageType::Notification;
            }
        }

        return MessageType::Error;
    }
};

/**
 * @brief Serializer for JSON-RPC messages
 */
class MessageSerializer {
public:
    /**
     * @brief Serialize a request to JSON string
     * @param req Request to serialize
     * @return JSON string
     */
    static std::string serialize(const JsonRpcRequest& req) {
        JsonValue obj = JsonValue::object();
        obj["jsonrpc"] = "2.0";
        obj["method"] = req.method;
        if (!req.id.is_null()) {
            obj["id"] = req.id;
        }
        if (!req.params.is_null()) {
            obj["params"] = req.params;
        }
        return obj.dump();
    }

    /**
     * @brief Serialize a response to JSON string
     * @param resp Response to serialize
     * @return JSON string
     */
    static std::string serialize(const JsonRpcResponse& resp) {
        JsonValue obj = JsonValue::object();
        obj["jsonrpc"] = "2.0";
        obj["id"] = resp.id;
        if (resp.is_error) {
            obj["error"] = resp.error;
        } else {
            obj["result"] = resp.result;
        }
        return obj.dump();
    }

    /**
     * @brief Serialize a message to JSON string
     * @param msg Message to serialize
     * @return JSON string
     */
    static std::string serialize(const JsonRpcMessage& msg) {
        if (msg.type() == MessageType::Request || msg.type() == MessageType::Notification) {
            return serialize(msg.request);
        } else if (msg.type() == MessageType::Response) {
            return serialize(msg.response);
        }
        return "";
    }
};

} // namespace mcpp
