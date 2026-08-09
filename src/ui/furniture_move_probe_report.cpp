#include "furniture_move_probe_report.hpp"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

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

std::string Hex(std::uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase << value;
    return output.str();
}

const char* RootName(std::uint8_t root_kind) {
    return root_kind == AC_MEW_FURNITURE_PROBE_UI
        ? "FurnitureBuildingUI" : "HouseSceneManager";
}

const char* StatusName(std::uint8_t status) {
    switch (status) {
    case AC_MEW_FURNITURE_PROBE_NODE_APPEARED:
        return "appeared";
    case AC_MEW_FURNITURE_PROBE_NODE_DISAPPEARED:
        return "disappeared";
    default:
        return "changed";
    }
}

nlohmann::json NodeDifference(
    const AcMewFurnitureProbeNodeDiff& difference) {
    auto ranges = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.range_count;
         ++index) {
        ranges.push_back({
            {"offset", difference.ranges[index].offset},
            {"length", difference.ranges[index].length}
        });
    }
    auto int32_candidates = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.int32_candidate_count;
         ++index) {
        const auto& candidate = difference.int32_candidates[index];
        int32_candidates.push_back({
            {"offset", candidate.offset},
            {"before", candidate.before},
            {"after", candidate.after}
        });
    }
    auto double_candidates = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.double_candidate_count;
         ++index) {
        const auto& candidate = difference.double_candidates[index];
        double_candidates.push_back({
            {"offset", candidate.offset},
            {"before", candidate.before},
            {"after", candidate.after}
        });
    }
    auto pointer_candidates = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.pointer_candidate_count;
         ++index) {
        const auto& candidate = difference.pointer_candidates[index];
        pointer_candidates.push_back({
            {"offset", candidate.offset},
            {"before", Hex(candidate.before)},
            {"after", Hex(candidate.after)},
            {"before_target_vtable_rva",
                candidate.before_target_vtable_rva == 0U
                    ? "" : Hex(candidate.before_target_vtable_rva)},
            {"after_target_vtable_rva",
                candidate.after_target_vtable_rva == 0U
                    ? "" : Hex(candidate.after_target_vtable_rva)}
        });
    }
    return {
        {"status", StatusName(difference.status)},
        {"address", Hex(difference.address)},
        {"before_vtable_rva",
            difference.before_vtable_rva == 0U
                ? "" : Hex(difference.before_vtable_rva)},
        {"after_vtable_rva",
            difference.after_vtable_rva == 0U
                ? "" : Hex(difference.after_vtable_rva)},
        {"parent_address", Hex(difference.parent_address)},
        {"parent_offset", difference.parent_offset},
        {"depth", difference.depth},
        {"before_byte_count", difference.before_byte_count},
        {"after_byte_count", difference.after_byte_count},
        {"changed_bytes", difference.changed_bytes},
        {"changed_ranges", std::move(ranges)},
        {"int32_candidates", std::move(int32_candidates)},
        {"double_candidates", std::move(double_candidates)},
        {"pointer_candidates", std::move(pointer_candidates)}
    };
}

nlohmann::json Difference(const AcMewFurnitureMoveDiff& difference) {
    auto nodes = nlohmann::json::array();
    for (std::uint32_t index = 0;
         index < difference.changed_node_count;
         ++index) {
        nodes.push_back(NodeDifference(difference.changed_nodes[index]));
    }
    return {
        {"root", RootName(difference.root_kind)},
        {"before_node_count", difference.before_node_count},
        {"after_node_count", difference.after_node_count},
        {"changed_node_count", difference.changed_node_count},
        {"changed_nodes", std::move(nodes)}
    };
}

}  // namespace

Result<std::filesystem::path> WriteFurnitureMoveProbeReport(
    const std::filesystem::path& diagnostics_root,
    const FurnitureMoveProbeReport& report) {
    std::error_code error;
    std::filesystem::create_directories(diagnostics_root, error);
    if (error) {
        return {{}, ErrorCode::BackupFailed,
            "could not create the diagnostics directory"};
    }

    nlohmann::json root{
        {"schema_version", 1},
        {"probe", "furniture_manual_move_object_graph_delta"},
        {"scene_generation", report.scene_generation},
        {"capture",
            "Press F7 before a manual move and F7 again after placement."},
        {"contents",
            "Bounded object graph deltas, offsets, vtable RVAs and numeric candidates; no raw memory bytes."},
        {"roots", nlohmann::json::array({
            Difference(report.furniture_ui),
            Difference(report.house_scene)})}
    };

    const auto filename =
        "furniture-move-probe-" + Timestamp() + ".json";
    const auto published = diagnostics_root / filename;
    const auto temporary = diagnostics_root / (filename + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << root.dump(2);
        output.flush();
        if (!output) {
            std::filesystem::remove(temporary, error);
            return {{}, ErrorCode::BackupFailed,
                "could not write the furniture diagnostics report"};
        }
    }
    if (!MoveFileExW(
            temporary.c_str(),
            published.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, error);
        return {{}, ErrorCode::BackupFailed,
            "could not publish the furniture diagnostics report"};
    }
    return {published};
}

}  // namespace autocattery::ui
