#include "auto_cattery/save_safety/atomic_file_replace.hpp"

#include <windows.h>

namespace autocattery::save_safety {
namespace {

bool IsRegularNonReparseFile(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool FlushFile(const std::filesystem::path& path) {
    HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    const bool flushed = FlushFileBuffers(handle) != FALSE;
    CloseHandle(handle);
    return flushed;
}

}  // namespace

Result<void> AtomicFileReplacer::ReplaceTemporary(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination) {
    std::error_code error;
    const auto temporary_parent =
        std::filesystem::weakly_canonical(temporary.parent_path(), error);
    const auto destination_parent = error
        ? std::filesystem::path{}
        : std::filesystem::weakly_canonical(destination.parent_path(), error);
    if (error || temporary_parent != destination_parent ||
        !IsRegularNonReparseFile(temporary) ||
        !IsRegularNonReparseFile(destination) ||
        !FlushFile(temporary)) {
        return {ErrorCode::WriteConflict,
            "atomic replacement preconditions are not satisfied"};
    }
    if (!ReplaceFileW(
            destination.c_str(), temporary.c_str(), nullptr,
            REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
        return {ErrorCode::WriteConflict,
            "atomic replacement failed; temporary file was retained"};
    }
    if (!FlushFile(destination)) {
        return {ErrorCode::WriteConflict,
            "atomic replacement succeeded but destination flush failed"};
    }
    return {};
}

}  // namespace autocattery::save_safety
