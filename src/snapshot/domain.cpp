#include "auto_cattery/snapshot/domain.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::snapshot {

bool SnapshotValidation::Valid() const noexcept {
    return ErrorCount() == 0;
}

std::size_t SnapshotValidation::WarningCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(),
        issues.end(),
        [](const ValidationIssue& issue) {
            return issue.severity == ValidationSeverity::Warning;
        }));
}

std::size_t SnapshotValidation::ErrorCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(),
        issues.end(),
        [](const ValidationIssue& issue) {
            return issue.severity == ValidationSeverity::Error;
        }));
}

SnapshotValidation Validate(const HouseSnapshot& snapshot) {
    SnapshotValidation validation;
    std::unordered_set<CatId> cat_ids;
    std::unordered_set<RoomId> room_ids;
    std::unordered_map<CatId, RoomId> resident_rooms;

    for (const auto& cat : snapshot.cats) {
        if (cat.id <= 0 || !cat_ids.insert(cat.id).second) {
            validation.issues.push_back({
                ValidationSeverity::Error,
                "duplicate-or-invalid-cat-id"
            });
        }
    }

    for (const auto& room : snapshot.rooms) {
        if (room.id.empty() || !room_ids.insert(room.id).second) {
            validation.issues.push_back({
                ValidationSeverity::Error,
                "duplicate-or-empty-room-id"
            });
        }
        for (const auto cat_id : room.residents) {
            if (!cat_ids.contains(cat_id)) {
                validation.issues.push_back({
                    ValidationSeverity::Error,
                    "room-references-missing-cat"
                });
            }
            const auto [iterator, inserted] =
                resident_rooms.emplace(cat_id, room.id);
            if (!inserted && iterator->second != room.id) {
                validation.issues.push_back({
                    ValidationSeverity::Error,
                    "cat-assigned-to-multiple-rooms"
                });
            }
        }
    }

    for (const auto& cat : snapshot.cats) {
        if (!cat.room_id) {
            continue;
        }
        if (!room_ids.contains(*cat.room_id)) {
            validation.issues.push_back({
                ValidationSeverity::Error,
                "cat-references-missing-room"
            });
            continue;
        }
        const auto resident = resident_rooms.find(cat.id);
        if (resident == resident_rooms.end() ||
            resident->second != *cat.room_id) {
            validation.issues.push_back({
                ValidationSeverity::Error,
                "cat-room-resident-mismatch"
            });
        }
    }

    if (!snapshot.capabilities.read_relationships) {
        validation.issues.push_back({
            ValidationSeverity::Warning,
            "relationships-unavailable"
        });
    }
    if (!snapshot.capabilities.read_room_capacities) {
        validation.issues.push_back({
            ValidationSeverity::Warning,
            "room-capacities-unavailable"
        });
    }
    if (!snapshot.capabilities.read_typed_abilities) {
        validation.issues.push_back({
            ValidationSeverity::Warning,
            "ability-types-unavailable"
        });
    }
    return validation;
}

}  // namespace autocattery::snapshot
