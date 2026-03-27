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
    const JsonRpcRequest* as_request() const;

    /**
     * @brief Get this message as a response (if applicable)
     * @return Pointer to response, or nullptr if not a response
     */
    const JsonRpcResponse* as_response() const;

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
    static Result<JsonRpcMessage> parse(const std::string& data);

    /**
     * @brief Parse a character buffer into a message
     * @param data Pointer to data buffer
     * @param len Length of data
     * @return Result containing parsed message or error
     */
    static Result<JsonRpcMessage> parse(const char* data, size_t len);

private:
    /**
     * @brief Detect message type from JSON object
     * @param obj JSON object to inspect
     * @return Detected message type
     */
    static MessageType detect_type(const JsonValue& obj);
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
    static std::string serialize(const JsonRpcRequest& req);

    /**
     * @brief Serialize a response to JSON string
     * @param resp Response to serialize
     * @return JSON string
     */
    static std::string serialize(const JsonRpcResponse& resp);

    /**
     * @brief Serialize a message to JSON string
     * @param msg Message to serialize
     * @return JSON string
     */
    static std::string serialize(const JsonRpcMessage& msg);
};

} // namespace mcpp
