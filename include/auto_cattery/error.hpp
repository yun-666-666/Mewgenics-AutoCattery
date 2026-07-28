#pragma once

#include <string>
#include <utility>

namespace autocattery {

enum class ErrorCode {
    Ok,
    NotInitialized,
    UnsupportedGameBuild,
    SceneUnavailable,
    UiNodeNotFound,
    CatDataUnavailable,
    RoomDataUnavailable,
    SnapshotInvalid,
    ConfigInvalid,
    BackupFailed,
    WriteConflict,
    OperationCancelled,
    PartialSuccess,
    InternalError
};

template<class T>
struct Result {
    T value{};
    ErrorCode code{ErrorCode::Ok};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code == ErrorCode::Ok;
    }
};

template<>
struct Result<void> {
    ErrorCode code{ErrorCode::Ok};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code == ErrorCode::Ok;
    }
};

}  // namespace autocattery
