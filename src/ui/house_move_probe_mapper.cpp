#include "house_move_probe_mapper.hpp"

#include <algorithm>
#include <chrono>

#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

namespace autocattery::ui {

void HouseMoveProbeMapper::Start(std::uint64_t scene_generation) {
    if (task_.valid()) {
        return;
    }
    requested_generation_ = scene_generation;
    canceled_ = false;
    task_ = std::async(std::launch::async, [scene_generation] {
        snapshot::SaveSnapshotAdapter adapter;
        return adapter.CaptureHouseSnapshotCandidates(scene_generation);
    });
}

void HouseMoveProbeMapper::Cancel() noexcept {
    canceled_ = true;
    requested_generation_ = 0;
}

bool HouseMoveProbeMapper::Loading() const noexcept {
    return task_.valid();
}

HouseMoveMappingResult HouseMoveProbeMapper::Poll(
    std::uint64_t scene_generation,
    void* house_scene_manager) {
    if (!task_.valid()) {
        return {};
    }
    if (task_.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready) {
        return {HouseMoveMappingStatus::Loading, {}, {}};
    }

    const auto captured = task_.get();
    if (canceled_ || requested_generation_ == 0 ||
        requested_generation_ != scene_generation) {
        return {HouseMoveMappingStatus::Rejected, {},
            "the House scene changed while mapping was loading"};
    }
    if (!captured || captured.value.empty() || !house_scene_manager) {
        return {HouseMoveMappingStatus::Rejected, {},
            "no readable save snapshot candidate is available"};
    }

    std::vector<AcMewHouseCatMatch> selected;
    std::size_t exact_candidates{};
    for (const auto& candidate : captured.value) {
        if (!candidate.capabilities.stable_cat_id ||
            candidate.scene_generation != scene_generation ||
            candidate.cats.empty()) {
            continue;
        }
        std::vector<std::int64_t> cat_ids;
        cat_ids.reserve(candidate.cats.size());
        for (const auto& cat : candidate.cats) {
            cat_ids.push_back(cat.id);
        }
        std::vector<AcMewHouseCatMatch> matches(candidate.cats.size());
        const auto identity = AcMewProbeHouseCatIdentity(
            house_scene_manager,
            cat_ids.data(),
            cat_ids.size(),
            matches.data(),
            matches.size());
        const bool exact =
            identity.stable_bijection != 0 &&
            identity.consistent_mapping != 0 &&
            identity.house_cat_count == candidate.cats.size() &&
            identity.match_count == candidate.cats.size();
        if (!exact) {
            continue;
        }
        const bool all_roots = std::all_of(
            matches.begin(),
            matches.end(),
            [](const AcMewHouseCatMatch& match) {
                return match.component != nullptr &&
                       match.root_node != nullptr;
            });
        if (!all_roots) {
            continue;
        }
        ++exact_candidates;
        selected = std::move(matches);
    }
    if (exact_candidates != 1U) {
        return {HouseMoveMappingStatus::Rejected, {},
            exact_candidates == 0U
                ? "no save candidate exactly matches every runtime HouseCat"
                : "multiple save candidates match the runtime HouseCats"};
    }
    return {
        HouseMoveMappingStatus::Ready,
        std::move(selected),
        {}
    };
}

}  // namespace autocattery::ui
