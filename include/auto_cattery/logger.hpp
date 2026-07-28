#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include "mewjector.h"

namespace autocattery {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static Logger& Instance();

    bool Initialize(const std::filesystem::path& log_directory);
    void AttachMewjector(const MewjectorAPI* api) noexcept;
    void SetGameBuild(std::string game_build);
    void Write(
        LogLevel level,
        std::string_view module,
        std::string_view event_id,
        std::string_view message) noexcept;

private:
    Logger() = default;

    std::mutex mutex_;
    std::filesystem::path log_path_;
    const MewjectorAPI* mewjector_{};
    std::string game_build_{"unknown"};
};

}  // namespace autocattery
