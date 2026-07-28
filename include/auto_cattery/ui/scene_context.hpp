#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "auto_cattery/error.hpp"

namespace autocattery::ui {

enum class UiContextKind {
    Unknown,
    House,
    EmbarkSelection,
    UnsafeTransition
};

struct UiContextSnapshot {
    UiContextKind kind{UiContextKind::Unknown};
    std::string scene_name;
    std::uint64_t scene_generation{};
    bool input_enabled{};
    bool save_in_progress{};
    std::vector<std::string> matched_signatures;
};

struct SceneSignatureRule {
    std::vector<std::string> scene_names;
    std::vector<std::string> required_nodes_any;
    std::vector<std::string> required_nodes_all;
    std::vector<std::string> forbidden_nodes;
    bool allow_structure_match{};
};

struct SceneSignatures {
    int schema_version{1};
    SceneSignatureRule house;
    SceneSignatureRule embark_selection;
};

Result<SceneSignatures> LoadSceneSignatures(
    const std::filesystem::path& path);

struct SceneObservation {
    UiContextKind kind{UiContextKind::Unknown};
    std::string scene_name;
    std::uintptr_t scene_instance{};
    bool scene_ready{};
    bool input_enabled{};
    bool save_in_progress{};
    std::vector<std::string> matched_signatures;
};

class SceneContextService {
public:
    using Callback = std::function<void(const UiContextSnapshot&)>;

    explicit SceneContextService(
        std::uint32_t stable_frames = 3,
        std::uint32_t missing_frames = 10);

    Result<void> Start();
    void Stop() noexcept;
    UiContextSnapshot Current() const;
    std::uint64_t Subscribe(Callback callback);
    void Unsubscribe(std::uint64_t token);
    Result<void> Observe(const SceneObservation& observation);

private:
    void Publish(
        UiContextSnapshot snapshot,
        std::vector<Callback>& callbacks);

    mutable std::mutex mutex_;
    bool started_{};
    std::uint32_t stable_frames_required_;
    std::uint32_t missing_frames_required_;
    std::uint32_t stable_frames_seen_{};
    std::uint32_t missing_frames_seen_{};
    std::uintptr_t pending_scene_instance_{};
    UiContextKind pending_kind_{UiContextKind::Unknown};
    std::string pending_scene_name_;
    std::uintptr_t active_scene_instance_{};
    UiContextSnapshot current_;
    std::uint64_t next_subscriber_token_{1};
    std::unordered_map<std::uint64_t, Callback> subscribers_;
};

const char* UiContextKindName(UiContextKind kind) noexcept;

}  // namespace autocattery::ui
