/**
 * @file prompt.cpp
 * @brief Prompt handler implementations
 */
#include "mcpp/server/prompt.hpp"

namespace mcpp {

// ============ PromptsListHandler ============

JsonRpcResponse PromptsListHandler::handle(const JsonRpcRequest& request) {
    JsonRpcResponse resp;
    resp.id = request.id;

    JsonValue prompts_array = JsonValue::array();
    for (const auto& prompt : prompts_) {
        JsonValue obj = JsonValue::object({
            {"name", prompt.name},
            {"description", prompt.description}
        });
        if (!prompt.arguments.is_null()) {
            obj["arguments"] = prompt.arguments;
        }
        prompts_array.push_back(obj);
    }

    resp.result = JsonValue::object({{"prompts", prompts_array}});
    resp.is_error = false;

    return resp;
}

void PromptsListHandler::add_prompt(const std::string& name,
                                    const std::string& description,
                                    const JsonValue& arguments,
                                    std::function<GetPromptResult(const JsonValue&)> handler) {
    Prompt prompt;
    prompt.name = name;
    prompt.description = description;
    prompt.arguments = arguments;

    prompts_.push_back(prompt);
    prompt_handlers_[name] = std::move(handler);
}

// ============ PromptsGetHandler ============

JsonRpcResponse PromptsGetHandler::handle(const JsonRpcRequest& request) {
    JsonRpcResponse resp;
    resp.id = request.id;

    if (!get_func_) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32603}, {"message", "Prompt retrieval not configured"}});
        return resp;
    }

    if (!request.params.is_object()) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32602}, {"message", "Invalid params: expected object"}});
        return resp;
    }

    std::string name = request.params.value("name", std::string());
    JsonValue arguments = request.params.value("arguments", JsonValue::object());

    if (name.empty()) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32602}, {"message", "Missing required parameter: name"}});
        return resp;
    }

    try {
        GetPromptResult result = get_func_(name, arguments);

        JsonValue resp_result = JsonValue::object({
            {"description", result.description}
        });

        JsonValue messages_array = JsonValue::array();
        for (const auto& msg : result.messages) {
            messages_array.push_back(JsonValue::object({
                {"role", msg.role},
                {"content", msg.content}
            }));
        }
        resp_result["messages"] = messages_array;

        resp.result = resp_result;
        resp.is_error = false;
    } catch (const std::exception& e) {
        resp.is_error = true;
        resp.error = JsonValue::object({{"code", -32603}, {"message", e.what()}});
    }

    return resp;
}

} // namespace mcpp
