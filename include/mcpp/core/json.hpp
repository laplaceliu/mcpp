/**
 * @file json.hpp
 * @brief JSON type definitions using nlohmann/json
 */
#pragma once

#include <nlohmann/json.hpp>

namespace mcpp {

/**
 * @brief JSON value type alias
 * @details Uses nlohmann::json as the underlying JSON implementation
 */
using Json = nlohmann::json;
using JsonValue = nlohmann::json;

/**
 * @brief Parse a JSON string
 * @param str JSON string to parse
 * @return Parsed JsonValue
 * @throws nlohmann::json::parse_error if parsing fails
 */
inline JsonValue parse_json(const std::string& str) {
    return JsonValue::parse(str);
}

} // namespace mcpp
