#include "auto_cattery/execution/plan_sealer.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <tuple>

#include "plan_sealer_snapshot.hpp"

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

template<class T>
void AppendOptional(
    std::ostringstream& stream,
    const std::optional<T>& value) {
    Append(stream, value.has_value());
    if (value) {
        Append(stream, *value);
    }
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
    detail::AppendSnapshotCapabilitiesAndBreeding(canonical, snapshot);

    auto cats = snapshot.cats;
    std::ranges::sort(cats, {}, &snapshot::CatSnapshot::id);
    for (const auto& cat : cats) {
        Append(canonical, cat.id);
        Append(canonical, cat.breed_id);
        Append(canonical, cat.voice_id);
        Append(canonical, cat.stat_type_id);
        Append(canonical, cat.class_id);
        for (const auto& value : cat.genetic_stats.values) {
            AppendOptional(canonical, value);
        }
        for (const auto& value : cat.heredity_bonus.values) {
            AppendOptional(canonical, value);
        }
        for (const auto& value : cat.equipment_bonus.values) {
            AppendOptional(canonical, value);
        }
        for (const auto& slot : cat.raw_ability_slots) {
            Append(canonical, slot.size());
            Append(canonical, slot);
        }
        for (const auto& trait : cat.visual_traits) {
            Append(canonical, trait.slot);
            Append(canonical, trait.category);
            Append(canonical, trait.id);
            Append(canonical, static_cast<int>(trait.kind));
        }
        AppendOptional(canonical, cat.birth_day);
        AppendOptional(canonical, cat.age_days);
        Append(canonical, cat.room_id.value_or(""));
        Append(canonical, cat.in_adventure_box);
        Append(canonical, static_cast<int>(cat.life_stage));
        Append(canonical, static_cast<int>(cat.available_for_combat));
        Append(canonical, static_cast<int>(cat.available_for_breeding));
        Append(canonical, static_cast<int>(cat.injured));
    }

    struct RoomDigest {
        std::string id;
        std::vector<snapshot::CatId> residents;
        std::optional<snapshot::RoomAttributes> attributes;
    };
    std::vector<RoomDigest> rooms;
    rooms.reserve(snapshot.rooms.size());
    for (const auto& room : snapshot.rooms) {
        auto residents = room.residents;
        std::ranges::sort(residents);
        rooms.push_back({room.id, std::move(residents), room.attributes});
    }
    std::ranges::sort(
        rooms,
        [](const auto& left, const auto& right) {
            return ByteLess(left.id, right.id);
        });
    for (const auto& room : rooms) {
        Append(canonical, room.id.size());
        Append(canonical, room.id);
        Append(canonical, room.attributes.has_value());
        if (room.attributes) {
            Append(canonical, room.attributes->comfort);
            Append(canonical, room.attributes->stimulation);
            Append(canonical, room.attributes->health);
            Append(canonical, room.attributes->mutation);
            Append(canonical, room.attributes->appeal);
        }
        for (const auto resident : room.residents) {
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
        AppendOptional(canonical, decision.breeding_partner_id);
        Append(canonical, decision.breeding_stats_stable);
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
