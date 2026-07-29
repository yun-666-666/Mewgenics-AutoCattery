#include "auto_cattery/execution/plan_sealer.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace autocattery::execution {
namespace {

std::uint64_t Hash(std::string_view text) noexcept {
    std::uint64_t value = 14695981039346656037ULL;
    for (const unsigned char byte : text) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value;
}

std::string Hex(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

template<class T>
void Append(std::ostringstream& stream, const T& value) {
    stream << value << '|';
}

bool ByteLess(std::string_view left, std::string_view right) {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(),
        [](const char left_byte, const char right_byte) {
            return static_cast<unsigned char>(left_byte) <
                   static_cast<unsigned char>(right_byte);
        });
}

}  // namespace

std::string DigestSnapshotContent(const snapshot::HouseSnapshot& snapshot) {
    std::ostringstream canonical;
    Append(canonical, snapshot.scene_generation);
    Append(canonical, snapshot.game_day.value_or(-1));
    Append(canonical, snapshot.capabilities.stable_cat_id);
    Append(canonical, snapshot.capabilities.read_room_assignments);

    std::vector<std::pair<snapshot::CatId, std::string>> cats;
    cats.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        cats.emplace_back(cat.id, cat.room_id.value_or(""));
    }
    std::ranges::sort(cats);
    for (const auto& [id, room] : cats) {
        Append(canonical, id);
        Append(canonical, room.size());
        Append(canonical, room);
    }

    std::vector<std::pair<std::string, std::vector<snapshot::CatId>>> rooms;
    rooms.reserve(snapshot.rooms.size());
    for (const auto& room : snapshot.rooms) {
        auto residents = room.residents;
        std::ranges::sort(residents);
        rooms.emplace_back(room.id, std::move(residents));
    }
    std::ranges::sort(
        rooms,
        [](const auto& left, const auto& right) {
            return ByteLess(left.first, right.first);
        });
    for (const auto& [id, residents] : rooms) {
        Append(canonical, id.size());
        Append(canonical, id);
        for (const auto resident : residents) {
            Append(canonical, resident);
        }
    }
    return Hex(Hash(canonical.str()));
}

std::string DigestClassification(
    const classification::ClassificationPlan& classification) {
    std::ostringstream canonical;
    Append(canonical, classification.algorithm_version);
    auto decisions = classification.decisions;
    std::ranges::sort(decisions, {}, &classification::CatDecision::cat_id);
    for (const auto& decision : decisions) {
        Append(canonical, decision.cat_id);
        Append(canonical, static_cast<int>(decision.primary_role));
        Append(canonical, decision.preview_cull_candidate);
        Append(canonical, decision.destructive_action_allowed);
        Append(canonical, static_cast<int>(decision.protection_level));
        Append(canonical, decision.move_allowed);
    }
    for (const auto id : classification.capacity_relief_candidates) {
        Append(canonical, id);
    }
    return Hex(Hash(canonical.str()));
}

CanonicalPlanDigest DigestRoomPlan(
    const room_planning::RoomPlan& plan) {
    std::ostringstream canonical;
    Append(canonical, plan.algorithm_version);
    Append(canonical, static_cast<int>(plan.disposition));
    Append(canonical, plan.minimum_capacity_relief_required);

    auto moves = plan.moves;
    std::ranges::sort(
        moves,
        [](const auto& left, const auto& right) {
            if (left.cat_id != right.cat_id) {
                return left.cat_id < right.cat_id;
            }
            if (left.from_room != right.from_room) {
                return ByteLess(left.from_room, right.from_room);
            }
            if (left.to_room != right.to_room) {
                return ByteLess(left.to_room, right.to_room);
            }
            return left.priority < right.priority;
        });
    for (const auto& move : moves) {
        Append(canonical, move.cat_id);
        Append(canonical, move.from_room.size());
        Append(canonical, move.from_room);
        Append(canonical, move.to_room.size());
        Append(canonical, move.to_room);
        Append(canonical, move.priority);
    }
    for (const auto& cull : plan.capacity_relief_suggestions) {
        Append(canonical, cull.cat_id);
        Append(canonical, cull.candidate_order);
    }
    return {Hex(Hash(canonical.str()))};
}

std::uint64_t BuildAuthorizationSeal(
    const OperationId& operation_id,
    const OperationPrecondition& precondition) noexcept {
    std::ostringstream canonical;
    Append(canonical, operation_id);
    Append(canonical, precondition.snapshot_content_digest);
    Append(canonical, precondition.classification_digest);
    Append(canonical, precondition.plan_digest.value);
    Append(canonical, precondition.protection_digest);
    Append(canonical, precondition.scene_generation);
    Append(canonical, precondition.game_day.value_or(-1));
    Append(canonical, precondition.game_build_identity);
    Append(canonical, precondition.save_identity);
    return Hash(canonical.str());
}

}  // namespace autocattery::execution
