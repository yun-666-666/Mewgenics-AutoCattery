#include "auto_cattery/save_safety/safe_path.hpp"

#include <windows.h>

#include <algorithm>

namespace autocattery::save_safety {
namespace {

bool HasReparsePoint(const std::filesystem::path& path) {
    std::filesystem::path current;
    for (const auto& component : path) {
        current /= component;
        const DWORD attributes = GetFileAttributesW(current.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return true;
        }
    }
    return false;
}

bool IsWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    auto expected = root.begin();
    auto actual = candidate.begin();
    for (; expected != root.end() && actual != candidate.end();
         ++expected, ++actual) {
        if (_wcsicmp(expected->c_str(), actual->c_str()) != 0) {
            return false;
        }
    }
    return expected == root.end();
}

}  // namespace

bool IsSafeOperationId(std::string_view value) noexcept {
    return !value.empty() && std::ranges::all_of(
        value, [](unsigned char byte) {
            return (byte >= 'a' && byte <= 'z') ||
                (byte >= 'A' && byte <= 'Z') ||
                (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '_';
        });
}

Result<std::filesystem::path> ResolveContainedExisting(
    const std::filesystem::path& root,
    std::string_view child_name) {
    if (!IsSafeOperationId(child_name)) {
        return {{}, ErrorCode::WriteConflict, "backup operation identity is unsafe"};
    }
    std::error_code error;
    if (HasReparsePoint(root)) {
        return {{}, ErrorCode::WriteConflict, "backup root is unavailable or unsafe"};
    }
    const auto canonical_root = std::filesystem::weakly_canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical_root, error) ||
        error || HasReparsePoint(canonical_root)) {
        return {{}, ErrorCode::WriteConflict, "backup root is unavailable or unsafe"};
    }
    const auto original_candidate = root / std::string(child_name);
    if (HasReparsePoint(original_candidate)) {
        return {{}, ErrorCode::WriteConflict,
            "backup directory is unavailable or unsafe"};
    }
    const auto candidate = std::filesystem::weakly_canonical(
        canonical_root / std::string(child_name), error);
    if (error || !std::filesystem::is_directory(candidate, error) || error ||
        HasReparsePoint(candidate) || !IsWithin(canonical_root, candidate)) {
        return {{}, ErrorCode::WriteConflict, "backup directory is unavailable or unsafe"};
    }
    return {candidate};
}

}  // namespace autocattery::save_safety
