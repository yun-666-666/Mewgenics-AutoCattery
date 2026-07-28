#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>

#include "auto_cattery/config.hpp"
#include "auto_cattery/logger.hpp"
#include "mew_ui_scene_probe.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::size_t kSceneProbeCapacity = 64;

bool Contains(const std::vector<std::string>& values, std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string TimestampForFilename() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d-%H%M%S");
    return output.str();
}

}  // namespace

struct MewUiBridge::RuntimeScene {
    void* manager{};
    std::string name;
    std::uint32_t component_count{};
    bool ready{};
};

const char* MewUiBridge::Name() const noexcept {
    return "MewUiBridge";
}

bool MewUiBridge::Initialize(const InitContext& context) {
    ready_logged_.store(false);
    last_tick_time_ = {};
    last_scene_summary_.clear();
    signatures_ = {};
    debug_probe_enabled_ = false;
    diagnostics_root_ = context.mod_root / L"diagnostics";

    const auto config = LoadConfig(
        context.mod_root / L"config" / L"default_config.json",
        context.mod_root / L"config" / L"user_config.json");
#ifdef _DEBUG
    debug_probe_enabled_ = true;
#else
    debug_probe_enabled_ =
        static_cast<bool>(config) && config.value.ui.show_debug_overlay;
#endif

    const auto signatures = LoadSceneSignatures(
        context.mod_root / L"config" / L"scene_signatures.json");
    if (signatures) {
        signatures_ = signatures.value;
    } else {
        Logger::Instance().Write(
            LogLevel::Error,
            Name(),
            "AC2201",
            "Scene signatures were rejected; scene detection remains fail-closed.");
    }

    if (!scene_context_.Start()) {
        Logger::Instance().Write(
            LogLevel::Error,
            Name(),
            "AC2202",
            "Scene context service failed to start.");
        return false;
    }
    scene_subscription_ = scene_context_.Subscribe([](const auto& snapshot) {
        std::ostringstream message;
        message << "Context=" << UiContextKindName(snapshot.kind)
                << " scene='" << snapshot.scene_name
                << "' generation=" << snapshot.scene_generation
                << " input=" << (snapshot.input_enabled ? 1 : 0)
                << " save=" << (snapshot.save_in_progress ? 1 : 0);
        Logger::Instance().Write(
            LogLevel::Info,
            "SceneContext",
            "AC2100",
            message.str());
    });

    started_ = MewUI_Start(
        "AutoCattery",
        30,
        100,
        100,
        &MewUiBridge::Tick,
        this) != 0;

    if (started_) {
        // The API's own per-scene callback trace is intentionally disabled:
        // AutoCattery emits a deduplicated summary instead.
        MewUI_SetDebugLogsEnabled(false);
        Logger::Instance().Write(
            LogLevel::Info,
            Name(),
            "AC1200",
            "MewUI bootstrap and phase 02 scene context service started.");
        return true;
    }

    scene_context_.Stop();
    Logger::Instance().Write(
        LogLevel::Error,
        Name(),
        "AC1201",
        "MewUI API bootstrap failed; UI is disabled and the mod is compatibility-degraded.");
    return true;
}

void MewUiBridge::Shutdown() noexcept {
    if (scene_subscription_ != 0) {
        scene_context_.Unsubscribe(scene_subscription_);
        scene_subscription_ = 0;
    }
    scene_context_.Stop();
    if (started_) {
        MewUI_Stop();
    }
    started_ = false;
    last_tick_time_ = {};
    last_scene_summary_.clear();
    ready_logged_.store(false);
}

bool MewUiBridge::Available() const noexcept {
    return started_;
}

bool MewUiBridge::Ready() const noexcept {
    return started_ && MewUI_IsReady() != 0;
}

SceneContextService& MewUiBridge::SceneContext() noexcept {
    return scene_context_;
}

void __cdecl MewUiBridge::Tick(void* user_data) {
    auto* bridge = static_cast<MewUiBridge*>(user_data);
    if (bridge == nullptr || MewUI_IsReady() == 0) {
        return;
    }
    bridge->OnTick();
}

void MewUiBridge::OnTick() {
    const auto now = std::chrono::steady_clock::now();
    if (last_tick_time_.time_since_epoch().count() != 0 &&
        now - last_tick_time_ < std::chrono::milliseconds(8)) {
        return;
    }
    last_tick_time_ = now;

    if (!ready_logged_.exchange(true)) {
        Logger::Instance().Write(
            LogLevel::Info,
            Name(),
            "AC1202",
            "MewUI API hooks are ready on the game UI thread.");
    }

    std::array<AcMewSceneRecord, kSceneProbeCapacity> records{};
    const auto count = AcMewEnumerateScenes(records.data(), records.size());
    std::vector<RuntimeScene> scenes;
    scenes.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        scenes.push_back({
            records[index].scene_manager,
            records[index].scene_name,
            records[index].component_count,
            records[index].ready != 0
        });
    }

    if (debug_probe_enabled_) {
        LogSceneSummary(scenes);
        if ((GetAsyncKeyState(VK_F8) & 1) != 0) {
            ExportSceneSummary(scenes);
        }
    }
    (void)scene_context_.Observe(ObserveScenes(scenes));
}

SceneObservation MewUiBridge::ObserveScenes(
    const std::vector<RuntimeScene>& scenes) const {
    const auto save_scene = std::find_if(
        scenes.begin(),
        scenes.end(),
        [](const RuntimeScene& scene) {
            return scene.ready &&
                   scene.name.find("Save") != std::string::npos;
        });
    if (save_scene != scenes.end()) {
        return {
            UiContextKind::UnsafeTransition,
            save_scene->name,
            reinterpret_cast<std::uintptr_t>(save_scene->manager),
            false,
            false,
            true,
            {"save-scene:" + save_scene->name}
        };
    }

    const auto match_rule =
        [&scenes](UiContextKind kind, const SceneSignatureRule& rule)
        -> std::vector<SceneObservation> {
        std::vector<SceneObservation> matches;
        const bool has_structure =
            !rule.required_nodes_any.empty() ||
            !rule.required_nodes_all.empty();
        for (const auto& scene : scenes) {
            const bool name_match = Contains(rule.scene_names, scene.name);
            if (!name_match &&
                !(rule.allow_structure_match && has_structure)) {
                continue;
            }

            std::vector<std::string> matched;
            if (name_match) {
                matched.push_back("scene:" + scene.name);
            }

            bool any_match = rule.required_nodes_any.empty();
            for (const auto& node : rule.required_nodes_any) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    any_match = true;
                    matched.push_back("node:" + node);
                }
            }

            bool all_match = true;
            for (const auto& node : rule.required_nodes_all) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    matched.push_back("node:" + node);
                } else {
                    all_match = false;
                }
            }

            bool forbidden_match = false;
            for (const auto& node : rule.forbidden_nodes) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    forbidden_match = true;
                    matched.push_back("forbidden:" + node);
                }
            }

            if (scene.ready && any_match && all_match && !forbidden_match) {
                matches.push_back({
                    kind,
                    scene.name,
                    reinterpret_cast<std::uintptr_t>(scene.manager),
                    true,
                    true,
                    false,
                    std::move(matched)
                });
            }
        }
        return matches;
    };

    auto house = match_rule(UiContextKind::House, signatures_.house);
    auto embark = match_rule(
        UiContextKind::EmbarkSelection,
        signatures_.embark_selection);
    if (house.size() + embark.size() != 1) {
        return {
            house.empty() && embark.empty()
                ? UiContextKind::Unknown
                : UiContextKind::UnsafeTransition,
            {},
            0,
            false,
            false,
            false,
            {"ambiguous-or-unmatched-scene"}
        };
    }
    return !house.empty() ? std::move(house.front())
                          : std::move(embark.front());
}

void MewUiBridge::LogSceneSummary(
    const std::vector<RuntimeScene>& scenes) {
    std::ostringstream summary;
    for (const auto& scene : scenes) {
        summary << scene.name << ':' << (scene.ready ? 1 : 0) << ';';
    }
    if (summary.str() == last_scene_summary_) {
        return;
    }
    last_scene_summary_ = summary.str();
    Logger::Instance().Write(
        LogLevel::Debug,
        "SceneProbe",
        "AC2400",
        "Loaded scene summary changed: " + last_scene_summary_);
}

void MewUiBridge::ExportSceneSummary(
    const std::vector<RuntimeScene>& scenes) const {
    std::error_code error;
    std::filesystem::create_directories(diagnostics_root_, error);
    if (error) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "SceneProbe",
            "AC2402",
            "Could not create diagnostics directory.");
        return;
    }

    nlohmann::json root{
        {"schema_version", 1},
        {"note", "Scene names and component counts only; no game assets exported."},
        {"scenes", nlohmann::json::array()}
    };
    for (const auto& scene : scenes) {
        root["scenes"].push_back({
            {"name", scene.name},
            {"ready", scene.ready},
            {"component_count", scene.component_count}
        });
    }

    const auto filename =
        "ui-scene-summary-" + TimestampForFilename() + ".json";
    std::ofstream output(diagnostics_root_ / filename, std::ios::trunc);
    output << root.dump(2);
    Logger::Instance().Write(
        LogLevel::Info,
        "SceneProbe",
        "AC2401",
        "Exported diagnostics scene summary: " + filename);
}

}  // namespace autocattery::ui
