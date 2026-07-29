#include "config_json.hpp"

#include <fstream>

namespace autocattery::config_detail {
namespace {

constexpr std::uintmax_t kMaximumConfigBytes = 1024U * 1024U;
constexpr std::size_t kMaximumObjectMembers = 1024;
constexpr std::size_t kMaximumDepth = 32;

Result<void> ValidateStructure(
    const Json& value,
    std::string path,
    std::size_t depth) {
    if (depth > kMaximumDepth) {
        return {ErrorCode::ConfigInvalid, path + " exceeds maximum nesting depth"};
    }
    if (value.is_array()) {
        return {ErrorCode::ConfigInvalid, path + " arrays are not supported"};
    }
    if (!value.is_object()) {
        return {};
    }
    if (value.size() > kMaximumObjectMembers) {
        return {ErrorCode::ConfigInvalid, path + " has too many entries"};
    }
    for (const auto& [key, child] : value.items()) {
        const auto child_path = path + "." + key;
        const auto valid = ValidateStructure(child, child_path, depth + 1);
        if (!valid) {
            return valid;
        }
    }
    return {};
}

}  // namespace

Result<Json> ReadJsonIfPresent(const std::filesystem::path& path) {
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error)) {
        return {Json::object()};
    }
    if (error) {
        return {{}, ErrorCode::ConfigInvalid, "cannot inspect configuration file"};
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return {{}, ErrorCode::ConfigInvalid, "cannot inspect configuration size"};
    }
    if (size > kMaximumConfigBytes) {
        return {{}, ErrorCode::ConfigInvalid, "configuration exceeds 1 MiB limit"};
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {{}, ErrorCode::ConfigInvalid, "cannot open configuration file"};
    }
    try {
        Json parsed;
        stream >> parsed;
        if (!parsed.is_object()) {
            return {{}, ErrorCode::ConfigInvalid, "configuration root must be an object"};
        }
        const auto structure = ValidateStructure(parsed, "$", 0);
        if (!structure) {
            return {{}, structure.code, structure.message};
        }
        return {std::move(parsed)};
    } catch (const Json::exception& exception) {
        return {{}, ErrorCode::ConfigInvalid, exception.what()};
    }
}

}  // namespace autocattery::config_detail
