#include "runtime_house_state.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace autocattery::ui {
namespace {

using Votes = std::unordered_map<
    snapshot::RoomId,
    std::unordered_map<RuntimePointer, std::size_t>>;

bool Allows(
    RuntimePointer pointer,
    const snapshot::RoomId& id,
    const RuntimeHouseState& runtime) {
    const auto evidence = std::ranges::find_if(
        runtime.rooms,
        [pointer](const auto& room) { return room.room == pointer; });
    return evidence == runtime.rooms.end() ||
        evidence->detected_ids.empty() ||
        std::ranges::find(evidence->detected_ids, id) !=
            evidence->detected_ids.end();
}

Result<void> ValidateCatIdentity(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime) {
    std::unordered_set<snapshot::CatId> expected;
    std::unordered_set<snapshot::CatId> observed;
    for (const auto& cat : snapshot.cats) {
        expected.insert(cat.id);
    }
    for (const auto& cat : runtime.cats) {
        if (cat.cat_id <= 0 || cat.component == 0 ||
            !observed.insert(cat.cat_id).second) {
            return {ErrorCode::SnapshotInvalid,
                    "runtime House cat identity is incomplete"};
        }
    }
    return expected == observed
        ? Result<void>{}
        : Result<void>{ErrorCode::SnapshotInvalid,
                       "runtime House cats changed"};
}

}  // namespace

bool RuntimeRoomIsDeliveryPipe(const RuntimeHouseState& runtime, RuntimePointer pointer) {
    const auto room = std::ranges::find(runtime.rooms, pointer, &RuntimeRoomEvidence::room);
    return room != runtime.rooms.end() && room->detected_ids.size() == 1U &&
        room->detected_ids.front() == "HousePipe";
}

Result<std::unordered_map<snapshot::RoomId, RuntimePointer>>
ResolveRuntimeRoomPointers(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime) {
    const auto identity = ValidateCatIdentity(snapshot, runtime);
    if (!identity) {
        return {{}, identity.code, identity.message};
    }
    std::vector<snapshot::RoomId> ids;
    std::unordered_set<snapshot::RoomId> id_set;
    for (const auto& room : snapshot.rooms) {
        // A saved delivery position is not an ordinary inhabitable room.
        if (room.id == "HousePipe") continue;
        if (!id_set.insert(room.id).second) {
            return {{}, ErrorCode::SnapshotInvalid,
                    "snapshot room identity is ambiguous"};
        }
        ids.push_back(room.id);
    }
    std::vector<RuntimePointer> pointers;
    for (const auto& cat : runtime.cats) {
        if (cat.room != 0 && !RuntimeRoomIsDeliveryPipe(runtime, cat.room) &&
            std::ranges::find(pointers, cat.room) == pointers.end()) {
            pointers.push_back(cat.room);
        }
    }
    for (const auto& evidence : runtime.rooms) {
        const bool relevant = std::ranges::any_of(
            evidence.detected_ids,
            [&id_set](const auto& id) { return id_set.contains(id); });
        if (relevant &&
            std::ranges::find(pointers, evidence.room) == pointers.end()) {
            pointers.push_back(evidence.room);
        }
    }
    if (ids.empty() || ids.size() != pointers.size() || ids.size() > 8U) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "runtime room identity is incomplete"};
    }

    Votes votes;
    std::unordered_map<snapshot::CatId, snapshot::RoomId> saved_rooms;
    for (const auto& cat : snapshot.cats) {
        if (cat.room_id) {
            saved_rooms.emplace(cat.id, *cat.room_id);
        }
    }
    for (const auto& cat : runtime.cats) {
        const auto saved = saved_rooms.find(cat.cat_id);
        if (cat.room != 0 && saved != saved_rooms.end()) {
            ++votes[saved->second][cat.room];
        }
    }

    std::sort(pointers.begin(), pointers.end());
    std::vector<RuntimePointer> best;
    std::size_t best_score{};
    bool found{};
    bool tied{};
    do {
        std::size_t score{};
        bool allowed = true;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            if (!Allows(pointers[index], ids[index], runtime)) {
                allowed = false;
                break;
            }
            score += votes[ids[index]][pointers[index]];
        }
        if (!allowed) {
            continue;
        }
        if (!found || score > best_score) {
            best = pointers;
            best_score = score;
            found = true;
            tied = false;
        } else if (score == best_score && pointers != best) {
            tied = true;
        }
    } while (std::next_permutation(pointers.begin(), pointers.end()));
    if (!found || tied) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "runtime room mapping is ambiguous"};
    }
    std::unordered_map<snapshot::RoomId, RuntimePointer> resolved;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        resolved.emplace(ids[index], best[index]);
    }
    return {std::move(resolved)};
}

}  // namespace autocattery::ui
