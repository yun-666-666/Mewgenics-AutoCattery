#include "auto_cattery/config_runtime.hpp"

#include <utility>

#include "auto_cattery/workflow/digests.hpp"

namespace autocattery {
namespace {

constexpr auto kReloadDebounce = std::chrono::milliseconds(500);

std::optional<std::filesystem::file_time_type> WriteTime(
    const std::filesystem::path& path) {
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error) || error) {
        return std::nullopt;
    }
    auto time = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    return time;
}

}  // namespace

RuntimeConfigService::RuntimeConfigService(
    std::filesystem::path default_path,
    std::filesystem::path user_path,
    Clock clock,
    InvalidationHandler invalidation_handler)
    : default_path_(std::move(default_path)),
      user_path_(std::move(user_path)),
      clock_(std::move(clock)),
      invalidation_handler_(std::move(invalidation_handler)) {
    if (!clock_) {
        clock_ = [] {
            return std::chrono::steady_clock::now();
        };
    }
    digest_ = workflow::DigestConfig(current_);
}

Config RuntimeConfigService::Current() const {
    return current_;
}

std::uint64_t RuntimeConfigService::Generation() const noexcept {
    return generation_;
}

std::string RuntimeConfigService::Digest() const {
    return digest_;
}

std::string RuntimeConfigService::LastError() const {
    return last_error_;
}

ConfigReloadResult RuntimeConfigService::LoadInitial() {
    const auto loaded = LoadConfig(default_path_, user_path_, session_override_);
    default_write_time_ = WriteTime(default_path_);
    user_write_time_ = WriteTime(user_path_);
    if (!loaded) {
        current_ = Config{};
        current_.force_read_only = true;
        current_.execution_safety.read_only_mode = true;
        current_.safety = current_.execution_safety;
        digest_ = workflow::DigestConfig(current_);
        last_error_ = loaded.message;
        return {
            ConfigReloadStatus::Rejected,
            loaded.code,
            loaded.message,
            generation_,
            digest_
        };
    }
    current_ = loaded.value;
    digest_ = workflow::DigestConfig(current_);
    last_error_.clear();
    return {
        ConfigReloadStatus::Applied,
        ErrorCode::Ok,
        "configuration loaded",
        generation_,
        digest_
    };
}

ConfigReloadResult RuntimeConfigService::RequestReload(
    workflow::WorkflowState state) {
    reload_pending_ = true;
    pending_ready_at_ = clock_();
    if (!Idle(state)) {
        return {
            ConfigReloadStatus::DeferredBusy,
            ErrorCode::WriteConflict,
            "configuration reload deferred until workflow is idle",
            generation_,
            digest_
        };
    }

    const auto loaded = LoadConfig(default_path_, user_path_, session_override_);
    default_write_time_ = WriteTime(default_path_);
    user_write_time_ = WriteTime(user_path_);
    if (!loaded) {
        reload_pending_ = false;
        last_error_ = loaded.message;
        return {
            ConfigReloadStatus::Rejected,
            loaded.code,
            loaded.message,
            generation_,
            digest_
        };
    }
    reload_pending_ = false;
    return ApplyIfChanged(loaded.value);
}

ConfigReloadResult RuntimeConfigService::PollHotReload(
    workflow::WorkflowState state) {
    NoticeFileChange();
    if (!reload_pending_) {
        return {
            ConfigReloadStatus::Unchanged,
            ErrorCode::Ok,
            {},
            generation_,
            digest_
        };
    }
    if (!Idle(state)) {
        return {
            ConfigReloadStatus::DeferredBusy,
            ErrorCode::WriteConflict,
            "configuration reload deferred until workflow is idle",
            generation_,
            digest_
        };
    }
    if (clock_() < pending_ready_at_) {
        return {
            ConfigReloadStatus::WaitingForDebounce,
            ErrorCode::Ok,
            "configuration file change is waiting for debounce",
            generation_,
            digest_
        };
    }
    return RequestReload(state);
}

ConfigReloadResult RuntimeConfigService::ApplySessionOverride(
    const SessionConfigOverride& session_override,
    workflow::WorkflowState state) {
    auto candidate = session_override_;
    session_override_ = candidate;
    MergeSessionOverride(session_override);
    const auto loaded = LoadConfig(default_path_, user_path_, session_override_);
    if (!loaded) {
        session_override_ = std::move(candidate);
        last_error_ = loaded.message;
        return {
            ConfigReloadStatus::Rejected,
            loaded.code,
            loaded.message,
            generation_,
            digest_
        };
    }
    reload_pending_ = true;
    pending_ready_at_ = clock_();
    if (!Idle(state)) {
        return {
            ConfigReloadStatus::DeferredBusy,
            ErrorCode::WriteConflict,
            "configuration reload deferred until workflow is idle",
            generation_,
            digest_
        };
    }
    reload_pending_ = false;
    return ApplyIfChanged(loaded.value);
}

ConfigReloadResult RuntimeConfigService::ApplyIfChanged(Config next) {
    const auto next_digest = workflow::DigestConfig(next);
    if (next_digest == digest_) {
        current_ = std::move(next);
        last_error_.clear();
        return {
            ConfigReloadStatus::Unchanged,
            ErrorCode::Ok,
            "configuration digest unchanged",
            generation_,
            digest_
        };
    }

    const auto previous = digest_;
    current_ = std::move(next);
    digest_ = next_digest;
    ++generation_;
    last_error_.clear();
    if (invalidation_handler_) {
        invalidation_handler_({generation_, previous, digest_});
    }
    return {
        ConfigReloadStatus::Applied,
        ErrorCode::Ok,
        "configuration applied and dependent caches invalidated",
        generation_,
        digest_
    };
}

bool RuntimeConfigService::Idle(workflow::WorkflowState state) const noexcept {
    return state == workflow::WorkflowState::Idle;
}

void RuntimeConfigService::NoticeFileChange() {
    const auto default_time = WriteTime(default_path_);
    const auto user_time = WriteTime(user_path_);
    if (default_time == default_write_time_ && user_time == user_write_time_) {
        return;
    }
    default_write_time_ = default_time;
    user_write_time_ = user_time;
    reload_pending_ = true;
    pending_ready_at_ = clock_() + kReloadDebounce;
}

void RuntimeConfigService::MergeSessionOverride(
    const SessionConfigOverride& update) {
#define AC_MERGE_SESSION_FIELD(name) \
    if (update.name) { \
        session_override_.name = update.name; \
    }
    AC_MERGE_SESSION_FIELD(combat_recommended_count)
    AC_MERGE_SESSION_FIELD(combat_exclude_injured)
    AC_MERGE_SESSION_FIELD(combat_stat_weights)
    AC_MERGE_SESSION_FIELD(breeding_stat_weights)
    AC_MERGE_SESSION_FIELD(minimum_general_reserve)
    AC_MERGE_SESSION_FIELD(allow_soft_overflow)
    AC_MERGE_SESSION_FIELD(read_only_mode)
    AC_MERGE_SESSION_FIELD(create_backup_before_apply)
    AC_MERGE_SESSION_FIELD(single_click_execute)
#undef AC_MERGE_SESSION_FIELD
}

}  // namespace autocattery
