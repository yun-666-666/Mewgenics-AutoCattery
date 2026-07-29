#include "auto_cattery/snapshot/detail/snapshot_assembler.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::snapshot::detail {

Result<HouseSnapshot> AssembleHouseSnapshot(
    std::uint64_t snapshot_id,
    std::uint64_t scene_generation,
    std::optional<std::int64_t> current_day,
    std::string source_save_name,
    std::vector<CatSnapshot> cats,
    std::span<const HouseStateEntry> house_entries) {
    HouseSnapshot snapshot;
    snapshot.snapshot_id = snapshot_id;
    snapshot.scene_generation = scene_generation;
    snapshot.game_day = current_day;
    snapshot.source_save_name = std::move(source_save_name);
    snapshot.cats = std::move(cats);
    snapshot.captured_at = std::chrono::system_clock::now();
    snapshot.capabilities = {
        .stable_cat_id = true,
        .read_display_name = true,
        .read_genetic_stats = true,
        .read_heredity_bonus = true,
        .read_equipment_bonus = true,
        .read_raw_ability_slots = true,
        .read_typed_abilities = true,
        .read_class_id = !snapshot.cats.empty() &&
            std::ranges::all_of(
                snapshot.cats,
                [](const CatSnapshot& cat) {
                    return !cat.class_id.empty();
                }),
        .read_age = false,
        .read_relationships = false,
        .read_room_assignments = true,
        .read_room_capacities = false
    };

    std::unordered_map<CatId, std::size_t> cat_index;
    for (std::size_t index = 0; index < snapshot.cats.size(); ++index) {
        if (!cat_index.emplace(snapshot.cats[index].id, index).second) {
            return {{}, ErrorCode::SnapshotInvalid, "duplicate cat ID"};
        }
    }

    std::map<RoomId, std::vector<CatId>> room_residents;
    std::unordered_set<CatId> assigned_cats;
    for (const auto& entry : house_entries) {
        const auto cat = cat_index.find(entry.cat_id);
        if (cat == cat_index.end()) {
            return {
                {},
                ErrorCode::SnapshotInvalid,
                "house state references a missing cat"
            };
        }
        if (!assigned_cats.insert(entry.cat_id).second) {
            return {
                {},
                ErrorCode::SnapshotInvalid,
                "cat appears more than once in house state"
            };
        }
        if (entry.room_id.empty()) {
            // The cat is still part of the current House UI candidate set, but
            // has no verifiable room assignment (for example, placed outside
            // a room). Preserve the cat with room_id unset.
            continue;
        }
        snapshot.cats[cat->second].room_id = entry.room_id;
        snapshot.cats[cat->second].in_adventure_box =
            entry.room_id == "AdventureBox";
        room_residents[entry.room_id].push_back(entry.cat_id);
    }
    std::erase_if(
        snapshot.cats,
        [&assigned_cats](const CatSnapshot& cat) {
            return !assigned_cats.contains(cat.id);
        });

    snapshot.rooms.reserve(room_residents.size());
    for (auto& [room_id, residents] : room_residents) {
        snapshot.rooms.push_back({
            .id = std::move(room_id),
            .residents = std::move(residents)
        });
    }
    const auto validation = Validate(snapshot);
    if (!validation.Valid()) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "assembled snapshot failed validation"
        };
    }
    return {std::move(snapshot)};
}

}  // namespace autocattery::snapshot::detail
