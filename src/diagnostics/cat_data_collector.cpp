#include "auto_cattery/diagnostics/cat_data_collector.hpp"

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "auto_cattery/version.hpp"

namespace autocattery::diagnostics {
namespace {

using Json = nlohmann::json;

const char* Sex(snapshot::CatSex value) {
    using enum snapshot::CatSex;
    switch (value) {
    case Female: return "female";
    case Male: return "male";
    default: return "unknown";
    }
}

const char* Sexuality(snapshot::CatSexuality value) {
    using enum snapshot::CatSexuality;
    switch (value) {
    case Straight: return "straight";
    case Bisexual: return "bisexual";
    case Gay: return "gay";
    default: return "unknown";
    }
}

const char* LifeStage(snapshot::LifeStage value) {
    using enum snapshot::LifeStage;
    switch (value) {
    case Kitten: return "kitten";
    case Adult: return "adult";
    case Senior: return "senior";
    case Dead: return "dead";
    default: return "unknown";
    }
}

const char* TriState(snapshot::TriState value) {
    using enum snapshot::TriState;
    switch (value) {
    case No: return "no";
    case Yes: return "yes";
    default: return "unknown";
    }
}

const char* Role(classification::CatRole value) {
    using enum classification::CatRole;
    switch (value) {
    case CombatRecommended: return "combat_recommended";
    case BreedingCore: return "breeding_core";
    case BreedingReserve: return "breeding_reserve";
    case GeneralReserve: return "general_reserve";
    case CullCandidate: return "cull_candidate";
    case ProtectedUnmanaged: return "protected_unmanaged";
    case Ineligible: return "ineligible";
    }
    return "unknown";
}

Json StatValues(const snapshot::StatBlock& block) {
    Json values = Json::array();
    for (const auto value : block.values) {
        values.push_back(value ? Json(*value) : Json(nullptr));
    }
    return values;
}

std::string Timestamp(std::chrono::system_clock::time_point value) {
    const auto time = std::chrono::system_clock::to_time_t(value);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

Json CatJson(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision* decision,
    const room_planning::PlannedMove* move) {
    Json result{
        {"cat_id", cat.id},
        {"breed_id", cat.breed_id},
        {"voice_id", cat.voice_id},
        {"sex", Sex(cat.sex)},
        {"sexuality", Sexuality(cat.sexuality)},
        {"sexuality_coefficient", cat.sexuality_coefficient},
        {"parent_a_id", cat.parent_a_id},
        {"parent_b_id", cat.parent_b_id},
        {"inbreeding_coefficient", cat.inbreeding_coefficient},
        {"stat_type_id", cat.stat_type_id},
        {"class_id", cat.class_id},
        {"genetic_stats", StatValues(cat.genetic_stats)},
        {"heredity_bonus", StatValues(cat.heredity_bonus)},
        {"equipment_bonus", StatValues(cat.equipment_bonus)},
        {"ability_slots", cat.raw_ability_slots},
        {"birth_day", cat.birth_day},
        {"age_days", cat.age_days},
        {"room_id", cat.room_id},
        {"in_adventure_box", cat.in_adventure_box},
        {"life_stage", LifeStage(cat.life_stage)},
        {"available_for_combat", TriState(cat.available_for_combat)},
        {"available_for_breeding", TriState(cat.available_for_breeding)},
        {"injured", TriState(cat.injured)}
    };
    result["visual_parts"] = Json::array();
    for (const auto& part : cat.raw_visual_part_slots) {
        result["visual_parts"].push_back({
            {"slot", part.slot}, {"category", part.category}, {"id", part.id}});
    }
    result["visual_traits"] = Json::array();
    for (const auto& trait : cat.visual_traits) {
        result["visual_traits"].push_back({
            {"slot", trait.slot},
            {"category", trait.category},
            {"id", trait.id},
            {"kind", trait.kind == snapshot::VisualTraitKind::Mutation
                ? "mutation" : "birth_defect"}
        });
    }
    if (decision != nullptr) {
        result["decision"] = {
            {"role", Role(decision->primary_role)},
            {"combat_score", decision->combat_score},
            {"breeding_score", decision->breeding_score},
            {"confidence", decision->confidence},
            {"breeding_partner_id", decision->breeding_partner_id},
            {"breeding_stats_stable", decision->breeding_stats_stable},
            {"move_allowed", decision->move_allowed},
            {"reasons", decision->reasons}
        };
    }
    if (move != nullptr) {
        result["planned_move"] = {
            {"from_room", move->from_room},
            {"to_room", move->to_room},
            {"reason", move->reason},
            {"priority", move->priority},
            {"executable", move->executable}
        };
    }
    return result;
}

}  // namespace

Result<std::filesystem::path> WriteCatDataSnapshot(
    const workflow::PreviewBundle& bundle,
    const std::filesystem::path& data_root) {
    if (data_root.empty()) {
        return {{}, ErrorCode::ConfigInvalid, "cat data root is empty"};
    }
    std::error_code error;
    std::filesystem::create_directories(data_root, error);
    if (error) {
        return {{}, ErrorCode::ConfigInvalid,
                "could not create AutoCatteryData"};
    }

    std::unordered_map<snapshot::CatId, const classification::CatDecision*>
        decisions;
    for (const auto& decision : bundle.classification.decisions) {
        decisions.emplace(decision.cat_id, &decision);
    }
    std::unordered_map<snapshot::CatId, const room_planning::PlannedMove*> moves;
    for (const auto& move : bundle.room_plan.moves) {
        moves.emplace(move.cat_id, &move);
    }

    Json output{
        {"schema_version", 1},
        {"mod", std::string(kModName)},
        {"mod_version", std::string(kModVersion)},
        {"captured_at_utc", Timestamp(bundle.snapshot.captured_at)},
        {"game_day", bundle.snapshot.game_day},
        {"snapshot_id", bundle.snapshot.snapshot_id},
        {"algorithm", bundle.room_plan.algorithm_version},
        {"privacy", {
            {"opt_in_required", true},
            {"excluded", Json::array({
                "cat_display_name", "save_name", "save_path", "account",
                "user_name", "machine_id"})}
        }},
        {"warnings", bundle.preview.warnings},
        {"rooms", Json::array()},
        {"cats", Json::array()}
    };
    for (const auto& room : bundle.snapshot.rooms) {
        Json item{{"room_id", room.id}, {"resident_count", room.residents.size()}};
        if (room.attributes) {
            item["attributes"] = {
                {"comfort", room.attributes->comfort},
                {"stimulation", room.attributes->stimulation},
                {"health", room.attributes->health},
                {"mutation", room.attributes->mutation},
                {"appeal", room.attributes->appeal}
            };
        }
        output["rooms"].push_back(std::move(item));
    }
    for (const auto& cat : bundle.snapshot.cats) {
        const auto decision = decisions.find(cat.id);
        const auto move = moves.find(cat.id);
        output["cats"].push_back(CatJson(
            cat,
            decision == decisions.end() ? nullptr : decision->second,
            move == moves.end() ? nullptr : move->second));
    }

    const auto digest = bundle.preview.bindings.snapshot_content_digest.empty()
        ? std::to_string(bundle.snapshot.snapshot_id)
        : bundle.preview.bindings.snapshot_content_digest;
    const auto published = data_root / ("cat-data-" + digest + ".json");
    auto temporary = published;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return {{}, ErrorCode::ConfigInvalid,
                    "could not create cat data snapshot"};
        }
        const auto serialized = output.dump(2) + "\n";
        stream.write(serialized.data(),
                     static_cast<std::streamsize>(serialized.size()));
        stream.flush();
        if (!stream) {
            std::filesystem::remove(temporary, error);
            return {{}, ErrorCode::ConfigInvalid,
                    "could not write cat data snapshot"};
        }
    }
    if (MoveFileExW(
            temporary.c_str(), published.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        std::filesystem::remove(temporary, error);
        return {{}, ErrorCode::ConfigInvalid,
                "could not publish cat data snapshot"};
    }
    return {published};
}

}  // namespace autocattery::diagnostics
