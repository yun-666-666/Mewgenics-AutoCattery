#include "auto_cattery/save_safety/save_stability.hpp"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <thread>

namespace autocattery::save_safety {
namespace {

struct FileSample {
    std::uintmax_t size{};
    std::filesystem::file_time_type modified{};

    bool operator==(const FileSample&) const = default;
};

Result<FileSample> Sample(const std::filesystem::path& path) {
    std::error_code error;
    FileSample sample;
    sample.size = std::filesystem::file_size(path, error);
    if (error) {
        return {{}, ErrorCode::BackupFailed, "save metadata is unavailable"};
    }
    sample.modified = std::filesystem::last_write_time(path, error);
    if (error) {
        return {{}, ErrorCode::BackupFailed, "save timestamp is unavailable"};
    }
    return {sample};
}

bool HasSidecar(const std::filesystem::path& path, std::error_code& error) {
    auto wal = path;
    wal += L"-wal";
    auto shm = path;
    shm += L"-shm";
    const bool found = std::filesystem::exists(wal, error);
    if (error || found) {
        return found;
    }
    return std::filesystem::exists(shm, error);
}

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

bool HasSavExtension(const std::filesystem::path& path) {
    auto extension = path.extension().wstring();
    std::ranges::transform(extension, extension.begin(), std::towlower);
    return extension == L".sav";
}

}  // namespace

StableSaveGuard::~StableSaveGuard() {
    Close();
}

StableSaveGuard::StableSaveGuard(StableSaveGuard&& other) noexcept
    : handle_(other.handle_),
      path_(std::move(other.path_)),
      byte_size_(other.byte_size_) {
    other.handle_ = nullptr;
}

StableSaveGuard& StableSaveGuard::operator=(StableSaveGuard&& other) noexcept {
    if (this != &other) {
        Close();
        handle_ = other.handle_;
        path_ = std::move(other.path_);
        byte_size_ = other.byte_size_;
        other.handle_ = nullptr;
    }
    return *this;
}

void StableSaveGuard::Close() noexcept {
    if (handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
}

Result<StableSaveGuard> StableSaveGuard::Acquire(
    const std::filesystem::path& save,
    std::chrono::milliseconds stable_window) {
    if (!HasSavExtension(save) || stable_window.count() < 0) {
        return {{}, ErrorCode::BackupFailed, "a .sav path and valid stable window are required"};
    }
    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(save, error);
    const DWORD attributes = error
        ? INVALID_FILE_ATTRIBUTES
        : GetFileAttributesW(path.c_str());
    if (error || HasReparsePoint(save) ||
        attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return {{}, ErrorCode::BackupFailed, "save is missing or unsafe"};
    }
    if (HasSidecar(path, error) || error) {
        return {{}, ErrorCode::BackupFailed, "SQLite WAL/SHM sidecar is present"};
    }

    const auto first = Sample(path);
    if (!first) {
        return {{}, first.code, first.message};
    }
    std::this_thread::sleep_for(stable_window);
    const auto second = Sample(path);
    if (!second || second.value != first.value) {
        return {{}, ErrorCode::BackupFailed, "save is still changing"};
    }

    HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        return {{}, ErrorCode::BackupFailed,
            code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION
                ? "save is occupied by another process"
                : "save read lock could not be acquired"};
    }

    const auto locked = Sample(path);
    if (!locked || locked.value != second.value ||
        HasSidecar(path, error) || error) {
        CloseHandle(handle);
        return {{}, ErrorCode::BackupFailed, "save changed before the read lock was acquired"};
    }
    StableSaveGuard guard;
    guard.handle_ = handle;
    guard.path_ = path;
    guard.byte_size_ = locked.value.size;
    return {std::move(guard)};
}

}  // namespace autocattery::save_safety
