#include "file_safety.hpp"

#include <windows.h>

#include <algorithm>

namespace autocattery::execution::detail {
namespace {

bool HasReparsePoint(const std::filesystem::path& path) {
    std::filesystem::path current;
    for (const auto& part : path) {
        current /= part;
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
    const std::filesystem::path& child) {
    auto root_part = root.begin();
    auto child_part = child.begin();
    for (; root_part != root.end() && child_part != child.end();
         ++root_part, ++child_part) {
        if (_wcsicmp(
                root_part->c_str(),
                child_part->c_str()) != 0) {
            return false;
        }
    }
    return root_part == root.end();
}

}  // namespace

bool IsSafeToken(std::string_view value) noexcept {
    return !value.empty() &&
           std::ranges::all_of(value, [](const unsigned char byte) {
               return (byte >= 'a' && byte <= 'z') ||
                      (byte >= 'A' && byte <= 'Z') ||
                      (byte >= '0' && byte <= '9') ||
                      byte == '-' || byte == '_';
           });
}

bool PrepareContainedDirectory(
    const std::filesystem::path& root,
    const std::filesystem::path& child,
    std::filesystem::path& canonical_root,
    std::filesystem::path& canonical_child) {
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error || HasReparsePoint(root)) {
        return false;
    }
    canonical_root = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return false;
    }
    const auto candidate = canonical_root / child;
    std::filesystem::create_directories(candidate, error);
    if (error || HasReparsePoint(candidate)) {
        return false;
    }
    canonical_child =
        std::filesystem::weakly_canonical(candidate, error);
    return !error && canonical_child != canonical_root &&
           IsWithin(canonical_root, canonical_child);
}

bool AtomicPublish(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    bool replace) {
    const DWORD flags =
        MOVEFILE_WRITE_THROUGH |
        (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    return MoveFileExW(
               temporary.c_str(), destination.c_str(), flags) != FALSE;
}

}  // namespace autocattery::execution::detail
