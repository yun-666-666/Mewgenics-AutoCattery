#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "auto_cattery/config.hpp"
#include "auto_cattery/workflow/domain.hpp"

namespace autocattery {

enum class ConfigReloadStatus {
    Unchanged,
    WaitingForDebounce,
    Applied,
    DeferredBusy,
    Rejected
};

struct ConfigReloadResult {
    ConfigReloadStatus status{ConfigReloadStatus::Unchanged};
    ErrorCode code{ErrorCode::Ok};
    std::string message;
    std::uint64_t generation{};
    std::string digest;
};

struct ConfigInvalidation {
    std::uint64_t generation{};
    std::string previous_digest;
    std::string new_digest;
};

class RuntimeConfigService final {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using InvalidationHandler = std::function<void(const ConfigInvalidation&)>;

    RuntimeConfigService(
        std::filesystem::path default_path,
        std::filesystem::path user_path,
        Clock clock = {},
        InvalidationHandler invalidation_handler = {});

    [[nodiscard]] Config Current() const;
    [[nodiscard]] std::uint64_t Generation() const noexcept;
    [[nodiscard]] std::string Digest() const;
    [[nodiscard]] std::string LastError() const;

    ConfigReloadResult LoadInitial();
    ConfigReloadResult RequestReload(workflow::WorkflowState state);
    ConfigReloadResult PollHotReload(workflow::WorkflowState state);
    ConfigReloadResult ApplySessionOverride(
        const SessionConfigOverride& session_override,
        workflow::WorkflowState state);

private:
    [[nodiscard]] ConfigReloadResult ApplyIfChanged(Config next);
    [[nodiscard]] bool Idle(workflow::WorkflowState state) const noexcept;
    void NoticeFileChange();
    void MergeSessionOverride(const SessionConfigOverride& update);

    std::filesystem::path default_path_;
    std::filesystem::path user_path_;
    Clock clock_;
    InvalidationHandler invalidation_handler_;
    Config current_;
    SessionConfigOverride session_override_;
    std::uint64_t generation_{1};
    std::string digest_;
    std::string last_error_;
    std::optional<std::filesystem::file_time_type> default_write_time_;
    std::optional<std::filesystem::file_time_type> user_write_time_;
    bool reload_pending_{};
    std::chrono::steady_clock::time_point pending_ready_at_{};
};

}  // namespace autocattery
