#include "auto_cattery/ui/scene_context.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace autocattery::ui {
namespace {

using Json = nlohmann::json;

Result<SceneSignatureRule> ParseRule(const Json& object) {
    SceneSignatureRule rule;
    try {
        rule.scene_names =
            object.value("scene_names", std::vector<std::string>{});
        rule.required_nodes_any =
            object.value("required_nodes_any", std::vector<std::string>{});
        rule.required_nodes_all =
            object.value("required_nodes_all", std::vector<std::string>{});
        rule.forbidden_nodes =
            object.value("forbidden_nodes", std::vector<std::string>{});
        rule.allow_structure_match =
            object.value("allow_structure_match", false);
    } catch (const Json::exception& error) {
        return {{}, ErrorCode::ConfigInvalid, error.what()};
    }
    return {std::move(rule), ErrorCode::Ok, {}};
}

bool IsReadyKind(UiContextKind kind) noexcept {
    return kind == UiContextKind::House ||
           kind == UiContextKind::EmbarkSelection;
}

}  // namespace

Result<SceneSignatures> LoadSceneSignatures(
    const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        return {
            {},
            ErrorCode::ConfigInvalid,
            "cannot open scene signature configuration"
        };
    }

    try {
        Json root;
        stream >> root;
        if (!root.is_object()) {
            return {
                {},
                ErrorCode::ConfigInvalid,
                "scene signature root must be an object"
            };
        }

        SceneSignatures signatures;
        signatures.schema_version = root.value("schema_version", 1);
        if (signatures.schema_version != 1) {
            return {
                {},
                ErrorCode::ConfigInvalid,
                "unsupported scene signature schema version"
            };
        }

        const auto house = ParseRule(root.at("house"));
        if (!house) {
            return {{}, house.code, "house: " + house.message};
        }
        const auto embark = ParseRule(root.at("embark_selection"));
        if (!embark) {
            return {
                {},
                embark.code,
                "embark_selection: " + embark.message
            };
        }

        signatures.house = house.value;
        signatures.embark_selection = embark.value;
        return {std::move(signatures), ErrorCode::Ok, {}};
    } catch (const Json::exception& error) {
        return {{}, ErrorCode::ConfigInvalid, error.what()};
    }
}

SceneContextService::SceneContextService(
    std::uint32_t stable_frames,
    std::uint32_t missing_frames)
    : stable_frames_required_(std::max(1U, stable_frames)),
      missing_frames_required_(std::max(1U, missing_frames)) {}

Result<void> SceneContextService::Start() {
    std::scoped_lock lock(mutex_);
    if (started_) {
        return {};
    }
    started_ = true;
    stable_frames_seen_ = 0;
    missing_frames_seen_ = 0;
    pending_scene_instance_ = 0;
    pending_kind_ = UiContextKind::Unknown;
    pending_scene_name_.clear();
    active_scene_instance_ = 0;
    current_ = {};
    return {};
}

void SceneContextService::Stop() noexcept {
    std::scoped_lock lock(mutex_);
    started_ = false;
    subscribers_.clear();
    stable_frames_seen_ = 0;
    missing_frames_seen_ = 0;
    pending_scene_instance_ = 0;
    active_scene_instance_ = 0;
    current_ = {};
}

UiContextSnapshot SceneContextService::Current() const {
    std::scoped_lock lock(mutex_);
    return current_;
}

std::uint64_t SceneContextService::Subscribe(Callback callback) {
    if (!callback) {
        return 0;
    }
    std::scoped_lock lock(mutex_);
    const auto token = next_subscriber_token_++;
    subscribers_.emplace(token, std::move(callback));
    return token;
}

void SceneContextService::Unsubscribe(std::uint64_t token) {
    std::scoped_lock lock(mutex_);
    subscribers_.erase(token);
}

Result<void> SceneContextService::Observe(
    const SceneObservation& observation) {
    std::vector<Callback> callbacks;
    UiContextSnapshot published;
    bool should_publish = false;

    {
        std::scoped_lock lock(mutex_);
        if (!started_) {
            return {
                ErrorCode::NotInitialized,
                "scene context service is not started"
            };
        }

        const bool ready =
            IsReadyKind(observation.kind) &&
            observation.scene_ready &&
            observation.input_enabled &&
            !observation.save_in_progress &&
            observation.scene_instance != 0;

        if (ready) {
            missing_frames_seen_ = 0;
            const bool same_pending =
                pending_kind_ == observation.kind &&
                pending_scene_instance_ == observation.scene_instance &&
                pending_scene_name_ == observation.scene_name;
            if (same_pending) {
                ++stable_frames_seen_;
            } else {
                pending_kind_ = observation.kind;
                pending_scene_instance_ = observation.scene_instance;
                pending_scene_name_ = observation.scene_name;
                stable_frames_seen_ = 1;
            }

            if (stable_frames_seen_ >= stable_frames_required_ &&
                (current_.kind != observation.kind ||
                 active_scene_instance_ != observation.scene_instance)) {
                ++current_.scene_generation;
                active_scene_instance_ = observation.scene_instance;
                published = {
                    observation.kind,
                    observation.scene_name,
                    current_.scene_generation,
                    true,
                    false,
                    observation.matched_signatures
                };
                should_publish = true;
            }
        } else {
            stable_frames_seen_ = 0;
            pending_kind_ = UiContextKind::Unknown;
            pending_scene_instance_ = 0;
            pending_scene_name_.clear();

            const bool immediate_unsafe =
                observation.save_in_progress ||
                observation.kind == UiContextKind::UnsafeTransition;
            if (IsReadyKind(current_.kind) &&
                (immediate_unsafe ||
                 ++missing_frames_seen_ >= missing_frames_required_)) {
                ++current_.scene_generation;
                active_scene_instance_ = 0;
                published = {
                    UiContextKind::UnsafeTransition,
                    observation.scene_name,
                    current_.scene_generation,
                    false,
                    observation.save_in_progress,
                    observation.matched_signatures
                };
                should_publish = true;
                missing_frames_seen_ = 0;
            } else if (current_.kind == UiContextKind::Unknown &&
                       immediate_unsafe) {
                ++current_.scene_generation;
                published = {
                    UiContextKind::UnsafeTransition,
                    observation.scene_name,
                    current_.scene_generation,
                    false,
                    observation.save_in_progress,
                    observation.matched_signatures
                };
                should_publish = true;
            }
        }

        if (should_publish) {
            Publish(std::move(published), callbacks);
            published = current_;
        }
    }

    for (const auto& callback : callbacks) {
        try {
            callback(published);
        } catch (...) {
            // Subscriber isolation is part of the service contract.
        }
    }
    return {};
}

void SceneContextService::Publish(
    UiContextSnapshot snapshot,
    std::vector<Callback>& callbacks) {
    current_ = std::move(snapshot);
    callbacks.reserve(subscribers_.size());
    for (const auto& [token, callback] : subscribers_) {
        (void)token;
        callbacks.push_back(callback);
    }
}

const char* UiContextKindName(UiContextKind kind) noexcept {
    switch (kind) {
    case UiContextKind::Unknown:
        return "Unknown";
    case UiContextKind::House:
        return "HouseReady";
    case UiContextKind::EmbarkSelection:
        return "EmbarkSelectionReady";
    case UiContextKind::UnsafeTransition:
        return "UnsafeTransition";
    }
    return "Unknown";
}

}  // namespace autocattery::ui
