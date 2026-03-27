#include "mcpp/protocol/response.hpp"

namespace mcpp {

JsonRpcResponse ResponseBuilder::success(const JsonValue& id, const JsonValue& result) {
    JsonRpcResponse resp;
    resp.id = id;
    resp.result = result;
    resp.is_error = false;
    return resp;
}

JsonRpcResponse ResponseBuilder::error(const JsonValue& id, int code, const std::string& message) {
    return error(id, code, message, JsonValue::object());
}

JsonRpcResponse ResponseBuilder::error(const JsonValue& id, ErrorCode code, const std::string& message) {
    return error(id, static_cast<int>(code), message);
}

JsonRpcResponse ResponseBuilder::error(const JsonValue& id, ErrorCode code, const std::string& message, const JsonValue& data) {
    return error(id, static_cast<int>(code), message, data);
}

JsonRpcResponse ResponseBuilder::error(const JsonValue& id, int code, const std::string& message, const JsonValue& data) {
    JsonRpcResponse resp;
    resp.id = id;
    resp.is_error = true;
    resp.error = {
        {"code", code},
        {"message", message}
    };
    if (!data.is_null()) {
        resp.error["data"] = data;
    }
    return resp;
}

} // namespace mcpp
