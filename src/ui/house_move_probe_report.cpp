#include "house_move_probe_report.hpp"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "mew_ui_house_move_probe.h"

namespace autocattery::ui {
namespace {

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &value);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d-%H%M%S");
    return output.str();
}

nlohmann::json RoomNames(std::uint32_t mask) {
    auto rooms = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < AC_MEW_MOVE_PROBE_ROOM_COUNT;
         ++index) {
        if ((mask & (1U << index)) != 0U) {
            rooms.push_back(AcMewMoveProbeRoomId(index));
        }
    }
    return rooms;
}

nlohmann::json Difference(
    const AcMewHouseMoveDiff& difference,
    std::size_t anonymous_ordinal) {
    const auto room_references =
        [](const AcMewMoveRoomReference* references, std::uint32_t count) {
            auto output = nlohmann::json::array();
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto& reference = references[index];
                output.push_back({
                    {"region",
                        reference.region == AC_MEW_MOVE_PROBE_COMPONENT
                            ? "component" : "root"},
                    {"offset", reference.offset},
                    {"room", AcMewMoveProbeRoomId(reference.room_index)},
                    {"storage",
                        reference.pointer_reference != 0
                            ? "pointer" : "inline"}
                });
            }
            return output;
        };
    auto candidates = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.double_candidate_count;
         ++index) {
        const auto& candidate = difference.double_candidates[index];
        candidates.push_back({
            {"region", candidate.region == AC_MEW_MOVE_PROBE_COMPONENT
                ? "component" : "root"},
            {"offset", candidate.offset},
            {"before", {
                candidate.before[0],
                candidate.before[1],
                candidate.before[2]}},
            {"after", {
                candidate.after[0],
                candidate.after[1],
                candidate.after[2]}}
        });
    }
    return {
        {"anonymous_cat_ordinal", anonymous_ordinal},
        {"component_changed_bytes", difference.component_changed_bytes},
        {"root_changed_bytes", difference.root_changed_bytes},
        {"component_rooms_before",
            RoomNames(difference.component_rooms_before)},
        {"component_rooms_after",
            RoomNames(difference.component_rooms_after)},
        {"root_rooms_before", RoomNames(difference.root_rooms_before)},
        {"root_rooms_after", RoomNames(difference.root_rooms_after)},
        {"room_references_before", room_references(
            difference.room_references_before,
            difference.room_references_before_count)},
        {"room_references_after", room_references(
            difference.room_references_after,
            difference.room_references_after_count)},
        {"coordinate_candidates", std::move(candidates)}
    };
}

}  // namespace

Result<std::filesystem::path> WriteHouseMoveProbeReport(
    const std::filesystem::path& diagnostics_root,
    const HouseMoveProbeReport& report) {
    std::error_code error;
    std::filesystem::create_directories(diagnostics_root, error);
    if (error) {
        return {{}, ErrorCode::BackupFailed,
            "could not create the diagnostics directory"};
    }

    nlohmann::json root{
        {"schema_version", 1},
        {"probe", "read_only_house_drag_delta"},
        {"scene_generation", report.scene_generation},
        {"privacy",
            "Only anonymous ordinals and bounded delta summaries are stored."},
        {"differences", nlohmann::json::array()}
    };
    for (std::size_t index = 0; index < report.differences.size(); ++index) {
        root["differences"].push_back(
            Difference(report.differences[index], index + 1U));
    }

    const auto filename =
        "house-move-probe-" + Timestamp() + ".json";
    const auto published = diagnostics_root / filename;
    const auto temporary = diagnostics_root / (filename + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << root.dump(2);
        output.flush();
        if (!output) {
            std::filesystem::remove(temporary, error);
            return {{}, ErrorCode::BackupFailed,
                "could not write the diagnostics report"};
        }
    }
    if (!MoveFileExW(
            temporary.c_str(),
            published.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, error);
        return {{}, ErrorCode::BackupFailed,
            "could not atomically publish the diagnostics report"};
    }
    return {published};
}

}  // namespace autocattery::ui
