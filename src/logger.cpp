#include "auto_cattery/logger.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#include "auto_cattery/version.hpp"
#include "mewjector.h"

namespace autocattery {
namespace {

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);

    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

}  // namespace

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

bool Logger::Initialize(const std::filesystem::path& log_directory) {
    std::scoped_lock lock(mutex_);
    std::error_code error;
    std::filesystem::create_directories(log_directory, error);
    if (error) {
        return false;
    }
    log_path_ = log_directory / "auto_cattery.log";
    std::ofstream stream(log_path_, std::ios::app);
    return static_cast<bool>(stream);
}

void Logger::AttachMewjector(const MewjectorAPI* api) noexcept {
    std::scoped_lock lock(mutex_);
    mewjector_ = api;
}

void Logger::SetGameBuild(std::string game_build) {
    std::scoped_lock lock(mutex_);
    game_build_ = std::move(game_build);
}

void Logger::Write(
    LogLevel level,
    std::string_view module,
    std::string_view event_id,
    std::string_view message) noexcept {
    try {
        std::scoped_lock lock(mutex_);
        std::ostringstream line;
        line << '[' << Timestamp() << ']'
             << "[thread=" << std::this_thread::get_id() << ']'
             << '[' << module << ']'
             << '[' << LevelName(level) << ']'
             << '[' << event_id << ']'
             << "[build=" << game_build_ << ']'
             << "[mod=" << kModVersion << "] "
             << message;

        if (!log_path_.empty()) {
            std::ofstream stream(log_path_, std::ios::app);
            stream << line.str() << '\n';
        }
        if (mewjector_ != nullptr && mewjector_->Log != nullptr) {
            mewjector_->Log("AutoCattery", "%s", line.str().c_str());
        }
    } catch (...) {
        // Logging must never terminate the host game.
    }
}

}  // namespace autocattery
