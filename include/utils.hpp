#pragma once
#include <nlohmann/json.hpp>
#include <initializer_list>
#include <string>

inline bool validate_schema_strict(const nlohmann::json& j, const std::initializer_list<std::pair<std::string, std::string>>& fields) {
    if (j.size() != fields.size()) return false;
    for (auto& [key, type] : fields) {
        if (!j.contains(key)) return false;
        if (type == "string" && !j[key].is_string()) return false;
        if (type == "object" && !j[key].is_object()) return false;
        if (type == "array" && !j[key].is_array()) return false;
    }
    return true;
}
