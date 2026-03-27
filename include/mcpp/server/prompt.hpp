/**
 * @file prompt.hpp
 * @brief Prompt management for MCP server
 */
#pragma once

#include <functional>
#include <vector>
#include "mcpp/core/types.hpp"
#include "mcpp/protocol/request.hpp"

namespace mcpp {

/**
 * @brief Handler for prompts/list request
 */
class PromptsListHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override;

    /**
     * @brief Register a prompt template
     * @param name Prompt name
     * @param description Prompt description
     * @param arguments Argument definitions
     * @param handler Prompt generator function
     */
    void add_prompt(const std::string& name,
                    const std::string& description,
                    const JsonValue& arguments,
                    std::function<GetPromptResult(const JsonValue&)> handler);

    const std::vector<Prompt>& prompts() const { return prompts_; }

private:
    std::vector<Prompt> prompts_;
    std::map<std::string, std::function<GetPromptResult(const JsonValue&)>> prompt_handlers_;
};

/**
 * @brief Handler for prompts/get request
 */
class PromptsGetHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override;

    /**
     * @brief Set the prompt generator function
     * @param handler Function that takes prompt name and args, returns GetPromptResult
     */
    void set_get_function(std::function<GetPromptResult(const std::string&, const JsonValue&)> handler) {
        get_func_ = std::move(handler);
    }

private:
    std::function<GetPromptResult(const std::string&, const JsonValue&)> get_func_;
};

} // namespace mcpp
