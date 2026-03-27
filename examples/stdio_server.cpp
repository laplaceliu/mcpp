/**
 * @file stdio_server.cpp
 * @brief Comprehensive example MCP server showcasing library features
 *
 * This server demonstrates:
 * - Tools: echo, calculator, file operations, image generation
 * - Resources: static config, dynamic system info, resource templates
 * - Prompts: reusable prompt templates with parameters
 * - Notifications: tools/resources changed notifications
 * - Logging: server-side logging messages
 * - Error handling: proper error responses
 * - Sampling: server requesting LLM completions from client
 */

#include "mcpp/server/server.hpp"
#include "mcpp/enterprise/ratelimit.hpp"
#include "mcpp/enterprise/metrics.hpp"
#include "mcpp/enterprise/circuit.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

using namespace mcpp;

// ============================================================================
// Tool Implementations
// ============================================================================

/**
 * @brief Echo tool - returns the input message
 */
CallToolResult echo_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    std::string message = args.value("message", std::string());
    std::string format = args.value("format", std::string("plain"));

    if (format == "uppercase") {
        for (char& c : message) {
            c = std::toupper(c);
        }
    } else if (format == "lowercase") {
        for (char& c : message) {
            c = std::tolower(c);
        }
    } else if (format == "reverse") {
        std::reverse(message.begin(), message.end());
    }

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", message}
    }));
    return result;
}

/**
 * @brief Calculator tool - performs arithmetic operations
 */
CallToolResult calculator_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    double a = args.value("a", 0.0);
    double b = args.value("b", 0.0);
    std::string operation = args.value("operation", std::string("add"));

    double calc_result = 0.0;
    std::string op_symbol;

    if (operation == "add") {
        calc_result = a + b;
        op_symbol = "+";
    } else if (operation == "subtract") {
        calc_result = a - b;
        op_symbol = "-";
    } else if (operation == "multiply") {
        calc_result = a * b;
        op_symbol = "*";
    } else if (operation == "divide") {
        if (b == 0.0) {
            result.is_error = true;
            result.error = "Division by zero";
            result.content.push_back(JsonValue::object({
                {"type", "text"},
                {"text", "Error: Division by zero"}
            }));
            return result;
        }
        calc_result = a / b;
        op_symbol = "/";
    } else if (operation == "power") {
        calc_result = std::pow(a, b);
        op_symbol = "^";
    } else if (operation == "modulo") {
        calc_result = std::fmod(a, b);
        op_symbol = "%";
    } else {
        result.is_error = true;
        result.error = "Unknown operation: " + operation;
        result.content.push_back(JsonValue::object({
            {"type", "text"},
            {"text", "Error: Unknown operation '" + operation + "'"}
        }));
        return result;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << a << " " << op_symbol << " " << b << " = " << calc_result;

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", oss.str()}
    }));
    return result;
}

/**
 * @brief Random number generator tool
 */
CallToolResult random_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    int min_val = args.value("min", 0);
    int max_val = args.value("max", 100);
    int count = args.value("count", 1);

    if (count < 1) count = 1;
    if (count > 100) count = 100;
    if (min_val > max_val) std::swap(min_val, max_val);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min_val, max_val);

    std::ostringstream oss;
    oss << "Random numbers [" << min_val << ", " << max_val << "]:\n";
    for (int i = 0; i < count; ++i) {
        oss << "  " << dis(gen);
        if (i < count - 1) oss << "\n";
    }

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", oss.str()}
    }));
    return result;
}

/**
 * @brief Time tool - returns current time in various formats
 */
CallToolResult time_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    std::string format = args.value("format", std::string("iso8601"));
    std::time_t now = std::time(nullptr);

    std::ostringstream oss;
    if (format == "iso8601") {
        oss << std::put_time(std::localtime(&now), "%Y-%m-%dT%H:%M:%S%z");
    } else if (format == "date") {
        oss << std::put_time(std::localtime(&now), "%Y-%m-%d");
    } else if (format == "time") {
        oss << std::put_time(std::localtime(&now), "%H:%M:%S");
    } else if (format == "unix") {
        oss << now;
    } else if (format == "custom") {
        std::string custom_format = args.value("pattern", std::string("%Y-%m-%d %H:%M:%S"));
        oss << std::put_time(std::localtime(&now), custom_format.c_str());
    } else {
        oss << std::put_time(std::localtime(&now), "%Y-%m-%dT%H:%M:%S%z");
    }

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", oss.str()}
    }));
    return result;
}

/**
 * @brief Text analysis tool
 */
CallToolResult analyze_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    std::string text = args.value("text", std::string());
    bool include_stats = args.value("include_stats", true);
    bool include_words = args.value("include_words", false);

    std::ostringstream oss;

    if (include_stats) {
        size_t char_count = text.length();
        size_t word_count = 0;
        size_t line_count = 0;
        size_t vowel_count = 0;
        size_t consonant_count = 0;
        size_t digit_count = 0;
        size_t space_count = 0;

        bool in_word = false;
        for (char c : text) {
            if (c == '\n') line_count++;
            if (c == ' ' || c == '\n' || c == '\t') space_count++;
            if (std::isdigit(c)) digit_count++;
            else if (std::isalpha(c)) {
                char lower = std::tolower(c);
                if (lower == 'a' || lower == 'e' || lower == 'i' || lower == 'o' || lower == 'u') {
                    vowel_count++;
                } else {
                    consonant_count++;
                }
                if (!in_word) {
                    word_count++;
                    in_word = true;
                }
            } else {
                in_word = false;
            }
        }
        if (!text.empty() && !std::isspace(text.back())) {
            // Last char is part of word
        }

        oss << "Text Statistics:\n";
        oss << "  Characters: " << char_count << "\n";
        oss << "  Words: " << word_count << "\n";
        oss << "  Lines: " << line_count + 1 << "\n";
        oss << "  Vowels: " << vowel_count << "\n";
        oss << "  Consonants: " << consonant_count << "\n";
        oss << "  Digits: " << digit_count << "\n";
        oss << "  Spaces: " << space_count << "\n";
        oss << "  Average word length: " << (word_count > 0 ? char_count / static_cast<double>(word_count) : 0) << "\n";
    }

    if (include_words && !text.empty()) {
        std::map<std::string, int> word_freq;
        std::string current_word;
        for (char c : text) {
            if (std::isalnum(c)) {
                current_word += std::tolower(c);
            } else if (!current_word.empty()) {
                word_freq[current_word]++;
                current_word.clear();
            }
        }
        if (!current_word.empty()) {
            word_freq[current_word]++;
        }

        if (!word_freq.empty()) {
            if (include_stats) oss << "\n";
            oss << "Top 10 Words:\n";
            std::vector<std::pair<std::string, int>> sorted_words(word_freq.begin(), word_freq.end());
            std::sort(sorted_words.begin(), sorted_words.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });

            int count = 0;
            for (const auto& wp : sorted_words) {
                if (count++ >= 10) break;
                oss << "  \"" << wp.first << "\": " << wp.second << "\n";
            }
        }
    }

    std::string output = oss.str();
    if (output.empty()) {
        output = "No analysis requested or empty text";
    }

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", output}
    }));
    return result;
}

/**
 * @brief JSON tool - manipulate JSON data
 */
CallToolResult json_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    std::string operation = args.value("operation", std::string("validate"));
    std::string json_str = args.value("data", std::string("{}"));

    std::ostringstream oss;

    if (operation == "validate") {
        try {
            JsonValue parsed = JsonValue::parse(json_str);
            oss << "Valid JSON\n";
            oss << "Type: " << parsed.type_name() << "\n";
            if (parsed.is_object()) {
                oss << "Keys: " << parsed.size() << "\n";
            } else if (parsed.is_array()) {
                oss << "Elements: " << parsed.size() << "\n";
            }
        } catch (const std::exception& e) {
            result.is_error = true;
            result.error = e.what();
            oss << "Invalid JSON: " << e.what() << "\n";
        }
    } else if (operation == "prettify") {
        try {
            JsonValue parsed = JsonValue::parse(json_str);
            oss << parsed.dump(2) << "\n";
        } catch (const std::exception& e) {
            result.is_error = true;
            result.error = e.what();
            oss << "Error: " << e.what() << "\n";
        }
    } else if (operation == "minify") {
        try {
            JsonValue parsed = JsonValue::parse(json_str);
            oss << parsed.dump() << "\n";
        } catch (const std::exception& e) {
            result.is_error = true;
            result.error = e.what();
            oss << "Error: " << e.what() << "\n";
        }
    } else if (operation == "keys") {
        try {
            JsonValue parsed = JsonValue::parse(json_str);
            if (parsed.is_object()) {
                oss << "Keys:\n";
                for (auto& el : parsed.items()) {
                    oss << "  " << el.key() << "\n";
                }
            } else {
                oss << "Not an object\n";
            }
        } catch (const std::exception& e) {
            result.is_error = true;
            result.error = e.what();
            oss << "Error: " << e.what() << "\n";
        }
    } else {
        result.is_error = true;
        result.error = "Unknown operation: " + operation;
        oss << "Error: Unknown operation '" << operation << "'\n";
    }

    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", oss.str()}
    }));
    return result;
}

/**
 * @brief Image placeholder tool (simulates image generation)
 */
CallToolResult image_tool(const std::string& name, const JsonValue& args) {
    MCPP_UNUSED(name);
    CallToolResult result;
    result.is_error = false;

    std::string prompt = args.value("prompt", std::string());
    std::string size = args.value("size", std::string("256x256"));

    std::ostringstream oss;
    oss << "Image Generation Request\n";
    oss << "Prompt: " << prompt << "\n";
    oss << "Size: " << size << "\n";
    oss << "Status: simulated (actual implementation would call image generation API)\n";

    // In a real implementation, this would call an image generation API
    // For now, we return a placeholder response
    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", oss.str()}
    }));

    // Could also return an image resource:
    // result.content.push_back(JsonValue::object({
    //     {"type", "image"},
    //     {"data", "base64_encoded_image_data"},
    //     {"mimeType", "image/png"}
    // }));

    return result;
}

// ============================================================================
// Resource Handlers
// ============================================================================

/**
 * @brief Get server configuration
 */
JsonValue get_config_resource() {
    JsonValue config = JsonValue::object();
    config["server_name"] = "comprehensive-mcp-server";
    config["version"] = "1.0.0";
    config["mcp_version"] = "2025-06-18";
    config["capabilities"] = JsonValue::object({
        {"tools", true},
        {"resources", true},
        {"prompts", true}
    });
    config["endpoints"] = JsonValue::object({
        {"stdio", true},
        {"http", false},
        {"websocket", false}
    });
    return config;
}

/**
 * @brief Get system information
 */
JsonValue get_system_resource() {
    JsonValue system = JsonValue::object();

    // OS info (simplified)
    system["os"] = "Linux";
    system["arch"] = "x86_64";

    // Current time
    std::time_t now = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S %z");
    system["timestamp"] = oss.str();
    system["uptime_seconds"] = 0;  // Would be actual uptime in real impl

    // Memory info (simulated)
    system["memory"] = JsonValue::object({
        {"total_mb", 16384},
        {"available_mb", 8192},
        {"used_percent", 50.0}
    });

    // CPU info (simulated)
    system["cpu"] = JsonValue::object({
        {"cores", 8},
        {"usage_percent", 25.0}
    });

    return system;
}

/**
 * @brief Get resource by URI
 */
JsonValue read_resource(const std::string& uri) {
    if (uri == "config://server") {
        return get_config_resource();
    } else if (uri == "system://info") {
        return get_system_resource();
    } else if (uri.find("user://") == 0) {
        // Template: user://{user_id}/profile
        std::string user_id = uri.substr(7);
        JsonValue user = JsonValue::object();
        user["id"] = user_id;
        user["name"] = "User " + user_id;
        user["email"] = user_id + "@example.com";
        user["created"] = "2024-01-01T00:00:00Z";
        return user;
    }

    // Default - return error info
    JsonValue error = JsonValue::object();
    error["error"] = "Unknown resource";
    error["uri"] = uri;
    return error;
}

// ============================================================================
// Prompt Handlers
// ============================================================================

/**
 * @brief Code review prompt
 */
GetPromptResult code_review_prompt(const JsonValue& args) {
    GetPromptResult result;
    result.description = "Generate a code review for the provided code";

    std::string language = args.value("language", std::string("unknown"));
    std::string code = args.value("code", std::string(""));
    std::string focus = args.value("focus", std::string("general"));

    PromptMessage msg1;
    msg1.role = "user";
    msg1.content = JsonValue::object({
        {"type", "text"},
        {"text", "Please review the following " + language + " code:"}
    });
    result.messages.push_back(msg1);

    PromptMessage msg2;
    msg2.role = "user";
    msg2.content = JsonValue::object({
        {"type", "text"},
        {"text", "```" + language + "\n" + code + "\n```"}
    });
    result.messages.push_back(msg2);

    PromptMessage msg3;
    msg3.role = "user";
    msg3.content = JsonValue::object({
        {"type", "text"},
        {"text", "Focus areas: " + focus + "\n\nProvide a detailed review including:\n1. Code quality and style\n2. Potential bugs or issues\n3. Performance considerations\n4. Security concerns\n5. Suggestions for improvement"}
    });
    result.messages.push_back(msg3);

    return result;
}

/**
 * @brief Documentation generation prompt
 */
GetPromptResult documentation_prompt(const JsonValue& args) {
    GetPromptResult result;
    result.description = "Generate documentation for code or API";

    std::string type = args.value("type", std::string("code"));
    std::string content = args.value("content", std::string(""));
    std::string format = args.value("format", std::string("markdown"));

    PromptMessage msg1;
    msg1.role = "user";
    msg1.content = JsonValue::object({
        {"type", "text"},
        {"text", "Generate " + format + " documentation for the following " + type + ":"}
    });
    result.messages.push_back(msg1);

    PromptMessage msg2;
    msg2.role = "user";
    msg2.content = JsonValue::object({
        {"type", "text"},
        {"text", content}
    });
    result.messages.push_back(msg2);

    PromptMessage msg3;
    msg3.role = "user";
    msg3.content = JsonValue::object({
        {"type", "text"},
        {"text", "Please generate comprehensive documentation including:\n- Overview and purpose\n- Usage examples\n- Parameter descriptions\n- Return values\n- Edge cases and error handling"}
    });
    result.messages.push_back(msg3);

    return result;
}

/**
 * @brief Test case generation prompt
 */
GetPromptResult test_generation_prompt(const JsonValue& args) {
    GetPromptResult result;
    result.description = "Generate unit tests for code";

    std::string language = args.value("language", std::string("unknown"));
    std::string code = args.value("code", std::string(""));
    std::string framework = args.value("framework", std::string("googleTest"));

    PromptMessage msg1;
    msg1.role = "user";
    msg1.content = JsonValue::object({
        {"type", "text"},
        {"text", "Generate unit tests for the following " + language + " code using " + framework + ":"}
    });
    result.messages.push_back(msg1);

    PromptMessage msg2;
    msg2.role = "user";
    msg2.content = JsonValue::object({
        {"type", "text"},
        {"text", "```" + language + "\n" + code + "\n```"}
    });
    result.messages.push_back(msg2);

    PromptMessage msg3;
    msg3.role = "user";
    msg3.content = JsonValue::object({
        {"type", "text"},
        {"text", "Generate comprehensive tests covering:\n1. Happy path scenarios\n2. Edge cases and boundary conditions\n3. Error handling\n4. Performance considerations if applicable"}
    });
    result.messages.push_back(msg3);

    return result;
}

// ============================================================================
// Server Setup
// ============================================================================

/**
 * @brief Register all tools with the server
 */
void register_tools(const std::shared_ptr<Server>& server) {
    // Echo tool with multiple format options
    server->register_tool(
        "echo",
        "Echo back the input message with optional transformations",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"message", JsonValue::object({
                    {"type", "string"},
                    {"description", "Message to echo"}
                })},
                {"format", JsonValue::object({
                    {"type", "string"},
                    {"description", "Output format: plain, uppercase, lowercase, reverse"},
                    {"enum", JsonValue::array({"plain", "uppercase", "lowercase", "reverse"})}
                })}
            })},
            {"required", JsonValue::array({"message"})}
        }),
        echo_tool
    );

    // Calculator tool
    server->register_tool(
        "calculator",
        "Perform arithmetic operations on two numbers",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"a", JsonValue::object({
                    {"type", "number"},
                    {"description", "First operand"}
                })},
                {"b", JsonValue::object({
                    {"type", "number"},
                    {"description", "Second operand"}
                })},
                {"operation", JsonValue::object({
                    {"type", "string"},
                    {"description", "Operation: add, subtract, multiply, divide, power, modulo"},
                    {"enum", JsonValue::array({"add", "subtract", "multiply", "divide", "power", "modulo"})}
                })}
            })},
            {"required", JsonValue::array({"a", "b", "operation"})}
        }),
        calculator_tool
    );

    // Random number generator
    server->register_tool(
        "random",
        "Generate random numbers within a specified range",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"min", JsonValue::object({
                    {"type", "integer"},
                    {"description", "Minimum value (default: 0)"}
                })},
                {"max", JsonValue::object({
                    {"type", "integer"},
                    {"description", "Maximum value (default: 100)"}
                })},
                {"count", JsonValue::object({
                    {"type", "integer"},
                    {"description", "Number of random values to generate (1-100, default: 1)"}
                })}
            })},
            {"required", JsonValue::array()}
        }),
        random_tool
    );

    // Time tool
    server->register_tool(
        "time",
        "Get current time in various formats",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"format", JsonValue::object({
                    {"type", "string"},
                    {"description", "Time format: iso8601, date, time, unix, custom"},
                    {"enum", JsonValue::array({"iso8601", "date", "time", "unix", "custom"})}
                })},
                {"pattern", JsonValue::object({
                    {"type", "string"},
                    {"description", "Custom strftime pattern (used when format=custom)"}
                })}
            })},
            {"required", JsonValue::array()}
        }),
        time_tool
    );

    // Text analysis tool
    server->register_tool(
        "analyze",
        "Analyze text and provide statistics",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"text", JsonValue::object({
                    {"type", "string"},
                    {"description", "Text to analyze"}
                })},
                {"include_stats", JsonValue::object({
                    {"type", "boolean"},
                    {"description", "Include character/word statistics (default: true)"}
                })},
                {"include_words", JsonValue::object({
                    {"type", "boolean"},
                    {"description", "Include word frequency analysis (default: false)"}
                })}
            })},
            {"required", JsonValue::array({"text"})}
        }),
        analyze_tool
    );

    // JSON manipulation tool
    server->register_tool(
        "json",
        "Manipulate and analyze JSON data",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"operation", JsonValue::object({
                    {"type", "string"},
                    {"description", "Operation: validate, prettify, minify, keys"},
                    {"enum", JsonValue::array({"validate", "prettify", "minify", "keys"})}
                })},
                {"data", JsonValue::object({
                    {"type", "string"},
                    {"description", "JSON string to process"}
                })}
            })},
            {"required", JsonValue::array({"operation", "data"})}
        }),
        json_tool
    );

    // Image generation tool (placeholder)
    server->register_tool(
        "generate_image",
        "Generate an image from a text prompt (simulated)",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"prompt", JsonValue::object({
                    {"type", "string"},
                    {"description", "Image generation prompt"}
                })},
                {"size", JsonValue::object({
                    {"type", "string"},
                    {"description", "Image size: 256x256, 512x512, 1024x1024"},
                    {"enum", JsonValue::array({"256x256", "512x512", "1024x1024"})}
                })}
            })},
            {"required", JsonValue::array({"prompt"})}
        }),
        image_tool
    );
}

/**
 * @brief Register all resources with the server
 */
void register_resources(const std::shared_ptr<Server>& server) {
    // Static configuration resource
    server->register_resource(
        "config://server",
        "Server Configuration",
        "Current server configuration and capabilities",
        "application/json",
        get_config_resource
    );

    // Dynamic system information resource
    server->register_resource(
        "system://info",
        "System Information",
        "Current system status and information",
        "application/json",
        get_system_resource
    );

    // Resource template for user profiles
    server->register_resource_template(
        "user://{user_id}/profile",
        "User Profile",
        "User profile information",
        "application/json"
    );
}

/**
 * @brief Register all prompts with the server
 */
void register_prompts(const std::shared_ptr<Server>& server) {
    // Code review prompt
    server->register_prompt(
        "code_review",
        "Generate a code review for provided code",
        [](const JsonValue& args) -> GetPromptResult {
            return code_review_prompt(args);
        }
    );

    // Documentation generation prompt
    server->register_prompt(
        "generate_docs",
        "Generate documentation for code or API",
        [](const JsonValue& args) -> GetPromptResult {
            return documentation_prompt(args);
        }
    );

    // Test generation prompt
    server->register_prompt(
        "generate_tests",
        "Generate unit tests for code",
        [](const JsonValue& args) -> GetPromptResult {
            return test_generation_prompt(args);
        }
    );
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "===============================================\n"
              << "   Comprehensive MCP Server Example\n"
              << "===============================================\n"
              << std::endl;

    // Create server options
    ServerOptions options;
    options.name = "comprehensive-server";
    options.version = "1.0.0";
    options.protocol_version = "2025-06-18";

    // Enable all capabilities
    options.capabilities.supports_tools = true;
    options.capabilities.supports_resources = true;
    options.capabilities.supports_prompts = true;
    options.capabilities.supports_logging = true;

    // Use stdio transport (default)
    options.transport_type = TransportFactory::Type::Stdio;

    // Create server
    auto server = std::make_shared<Server>(options);

    // Register all features
    std::cout << "Registering tools...\n";
    register_tools(server);

    std::cout << "Registering resources...\n";
    register_resources(server);

    std::cout << "Registering prompts...\n";
    register_prompts(server);

    // Log server startup
    std::cout << "\n===============================================\n"
              << "   Server Capabilities:\n"
              << "   - Tools: echo, calculator, random, time,\n"
              << "            analyze, json, generate_image\n"
              << "   - Resources: config://, system://, user://\n"
              << "   - Prompts: code_review, generate_docs,\n"
              << "              generate_tests\n"
              << "===============================================\n"
              << std::endl;

    // Start server
    std::cout << "Starting server...\n" << std::endl;
    if (!server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started successfully. Waiting for requests...\n"
              << "(Press Ctrl+C to stop)\n"
              << std::endl;

    // Run until shutdown
    server->wait();

    std::cout << "\nServer stopped." << std::endl;
    return 0;
}