#include "in_game_panel_controller.hpp"

#include <vector>

#include "mew_ui_house_cat_probe.h"

namespace autocattery::ui {

void InGamePanelController::ResolveCurrentSave() {
    current_save_.reset();
    current_save_checked_ = house_scene_manager_ != nullptr;
    if (!current_save_checked_ || !protection_) return;

    std::size_t exact_matches{};
    for (std::size_t index = 0; index < protection_->saves().size(); ++index) {
        const auto& snapshot = protection_->saves()[index].snapshot;
        if (!snapshot.capabilities.stable_cat_id || snapshot.cats.empty()) {
            continue;
        }
        std::vector<std::int64_t> cat_ids;
        cat_ids.reserve(snapshot.cats.size());
        for (const auto& cat : snapshot.cats) cat_ids.push_back(cat.id);

        std::vector<AcMewHouseCatMatch> matches(snapshot.cats.size());
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
}

}  // namespace autocattery::ui
