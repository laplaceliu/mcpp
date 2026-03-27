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
    JsonRpcResponse handle(const JsonRpcRequest& request) override;

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
                      std::function<JsonValue()> handler);

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
                               const std::string& mime_type);

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
    JsonRpcResponse handle(const JsonRpcRequest& request) override;

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
    JsonRpcResponse handle(const JsonRpcRequest& request) override;
};

} // namespace mcpp
