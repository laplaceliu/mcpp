#include "mcpp/protocol/message.hpp"
#include "mcpp/core/error.hpp"

namespace mcpp {

const JsonRpcRequest* JsonRpcMessage::as_request() const {
    return type_ == MessageType::Request ? &request : nullptr;
}

const JsonRpcResponse* JsonRpcMessage::as_response() const {
    return type_ == MessageType::Response ? &response : nullptr;
}

MessageType MessageParser::detect_type(const JsonValue& obj) {
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

Result<JsonRpcMessage> MessageParser::parse(const std::string& data) {
    return parse(data.c_str(), data.size());
}

Result<JsonRpcMessage> MessageParser::parse(const char* data, size_t len) {
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

std::string MessageSerializer::serialize(const JsonRpcRequest& req) {
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

std::string MessageSerializer::serialize(const JsonRpcResponse& resp) {
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

std::string MessageSerializer::serialize(const JsonRpcMessage& msg) {
    if (msg.type() == MessageType::Request || msg.type() == MessageType::Notification) {
        return serialize(msg.request);
    } else if (msg.type() == MessageType::Response) {
        return serialize(msg.response);
    }
    return "";
}

} // namespace mcpp
