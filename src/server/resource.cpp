/**
 * @file resource.cpp
 * @brief Resource handler implementations
 */
#include "mcpp/server/resource.hpp"

namespace mcpp {

JsonRpcResponse ResourcesListHandler::handle(const JsonRpcRequest& request) {
    JsonRpcResponse resp;
    resp.id = request.id;

    JsonValue result = JsonValue::object();

    // Serialize resources
    JsonValue resources_array = JsonValue::array();
    for (const auto& res : resources_) {
        resources_array.push_back(JsonValue::object({
            {"uri", res.uri},
            {"name", res.name},
            {"description", res.description},
            {"mimeType", res.mime_type}
        }));
    }

    // Serialize resource templates
    JsonValue templates_array = JsonValue::array();
    for (const auto& tmpl : templates_) {
        templates_array.push_back(JsonValue::object({
            {"uriTemplate", tmpl.uri_template},
            {"name", tmpl.name},
            {"description", tmpl.description},
            {"mimeType", tmpl.mime_type}
        }));
    }

    result["resources"] = resources_array;
    result["resourceTemplates"] = templates_array;

    resp.result = result;
    resp.is_error = false;

    return resp;
}

void ResourcesListHandler::add_resource(const std::string& uri,
                                        const std::string& name,
                                        const std::string& description,
                                        const std::string& mime_type,
                                        std::function<JsonValue()> handler) {
    Resource res;
    res.uri = uri;
    res.name = name;
    res.description = description;
    res.mime_type = mime_type;

    resources_.push_back(res);
    resource_handlers_[uri] = std::move(handler);
}

void ResourcesListHandler::add_resource_template(const std::string& uri_template,
                                                  const std::string& name,
                                                  const std::string& description,
                                                  const std::string& mime_type) {
    ResourceTemplate tmpl;
    tmpl.uri_template = uri_template;
    tmpl.name = name;
    tmpl.description = description;
    tmpl.mime_type = mime_type;

    templates_.push_back(tmpl);
}

// ============ ResourceReadHandler ============

JsonRpcResponse ResourceReadHandler::handle(const JsonRpcRequest& request) {
    JsonRpcResponse resp;
    resp.id = request.id;

    if (!read_func_) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32603}, {"message", "Resource reading not configured"}});
        return resp;
    }

    if (!request.params.is_object() || !request.params.contains("uri")) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32602}, {"message", "Missing required parameter: uri"}});
        return resp;
    }

    std::string uri = request.params.value("uri", std::string());
    if (uri.empty()) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32602}, {"message", "uri cannot be empty"}});
        return resp;
    }

    try {
        JsonValue contents = read_func_(uri);

        JsonValue result = JsonValue::object();
        if (contents.is_array()) {
            result["contents"] = contents;
        } else {
            result["contents"] = JsonValue::array({contents});
        }

        resp.result = result;
        resp.is_error = false;
    } catch (const std::exception& e) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32603}, {"message", e.what()}});
    }

    return resp;
}

// ============ ResourceSubscribeHandler ============

JsonRpcResponse ResourceSubscribeHandler::handle(const JsonRpcRequest& request) {
    JsonRpcResponse resp;
    resp.id = request.id;

    // Subscription is not yet implemented - just acknowledge
    resp.result = JsonValue::object({{"subscribed", true}});
    resp.is_error = false;

    return resp;
}

} // namespace mcpp
