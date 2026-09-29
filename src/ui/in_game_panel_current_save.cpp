#include "in_game_panel_controller.hpp"

#include <chrono>
#include <vector>

#include "auto_cattery/logger.hpp"
#include "mew_ui_house_cat_probe.h"

namespace autocattery::ui {

void InGamePanelController::ResolveCurrentSave() {
    const auto started = std::chrono::steady_clock::now();
    current_save_.reset();
    current_save_checked_ = house_scene_manager_ != nullptr;
    if (!current_save_checked_ || !protection_) {
        status_ = English() ? "House context unavailable; refresh in the cat house"
                            : "猫舍场景尚未就绪，请在猫舍内刷新";
        Logger::Instance().Write(LogLevel::Warn, "ManagementPanel", "AC18005", status_);
        return;
    }

    const auto runtime_cat_count = AcMewCountHouseCats(house_scene_manager_);
    std::size_t candidate_count{};
    std::size_t cat_count_matches{};
    std::size_t identity_probes{};
    std::size_t exact_matches{};
    for (std::size_t index = 0; index < protection_->saves().size(); ++index) {
        const auto& snapshot = protection_->saves()[index].snapshot;
        if (!snapshot.capabilities.stable_cat_id || snapshot.cats.empty()) {
            continue;
        }
        ++candidate_count;
        if (snapshot.cats.size() != runtime_cat_count) {
            continue;
        }
        ++cat_count_matches;
        std::vector<std::int64_t> cat_ids;
        cat_ids.reserve(snapshot.cats.size());
        for (const auto& cat : snapshot.cats) cat_ids.push_back(cat.id);

        std::vector<AcMewHouseCatMatch> matches(snapshot.cats.size());
        ++identity_probes;
        const auto identity = AcMewProbeHouseCatIdentity(
            house_scene_manager_, cat_ids.data(), cat_ids.size(),
            matches.data(), matches.size());
        const bool exact = identity.stable_bijection != 0 &&
            identity.consistent_mapping != 0 &&
            identity.house_cat_count == snapshot.cats.size() &&
            identity.match_count == snapshot.cats.size();
        if (!exact) continue;
        ++exact_matches;
        current_save_ = index;
    }
    if (exact_matches != 1U) current_save_.reset();
    if (!current_save_) {
        status_ = English()
            ? "Saved cats do not uniquely match the current house; save normally, then refresh"
            : "已保存猫群与当前猫舍未唯一匹配，请正常保存后刷新";
    }

    const auto elapsed_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18005",
        "Current save resolution completed; elapsed_us=" +
            std::to_string(elapsed_us) +
            " candidates=" + std::to_string(candidate_count) +
            " cat_count_matches=" + std::to_string(cat_count_matches) +
            " identity_probes=" + std::to_string(identity_probes) +
            " exact_matches=" + std::to_string(exact_matches));
}

}  // namespace autocattery::ui
