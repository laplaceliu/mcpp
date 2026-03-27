/**
 * @file resource.hpp
 * @brief Resource management for MCP server
 */
#pragma once

#include <functional>
#include <map>
#include <regex>
#include "mcpp/core/types.hpp"
#include "mcpp/protocol/request.hpp"

namespace mcpp {

/**
 * @brief Handler for resources/list request
 */
class ResourcesListHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override {
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

    /**
     * @brief Register a resource
     * @param uri Resource URI
     * @param name Resource name
     * @param description Resource description
     * @param mime_type MIME type
     * @param handler Reader function
     */
    void add_resource(const std::string& uri,
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

    /**
     * @brief Register a resource template
     * @param uri_template URI template pattern
     * @param name Template name
     * @param description Template description
     * @param mime_type MIME type
     */
    void add_resource_template(const std::string& uri_template,
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

    const std::vector<Resource>& resources() const { return resources_; }
    const std::vector<ResourceTemplate>& resource_templates() const { return templates_; }

private:
    std::vector<Resource> resources_;
    std::vector<ResourceTemplate> templates_;
    std::map<std::string, std::function<JsonValue()>> resource_handlers_;
};

/**
 * @brief Handler for resources/read request
 */
class ResourceReadHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override {
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

    /**
     * @brief Set the resource reader function
     * @param handler Function that takes URI and returns resource content
     */
    void set_read_function(std::function<JsonValue(const std::string&)> handler) {
        read_func_ = std::move(handler);
    }

private:
    std::function<JsonValue(const std::string&)> read_func_;
};

/**
 * @brief Handler for resources/subscribe request
 */
class ResourceSubscribeHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        JsonRpcResponse resp;
        resp.id = request.id;

        // Subscription is not yet implemented - just acknowledge
        resp.result = JsonValue::object({{"subscribed", true}});
        resp.is_error = false;

        return resp;
    }
};

} // namespace mcpp
