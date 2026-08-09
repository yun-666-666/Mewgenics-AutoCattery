#include "furniture_move_probe_report.hpp"
#include "mew_ui_furniture_move_probe.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

void ProbeVtableAnchor() {}

const AcMewFurnitureProbeNodeDiff* FindChangedNode(
    const AcMewFurnitureMoveDiff& difference,
    std::uint64_t address) {
    for (std::uint32_t index = 0;
         index < difference.changed_node_count;
         ++index) {
        if (difference.changed_nodes[index].address == address) {
            return &difference.changed_nodes[index];
        }
    }
    return nullptr;
}

}  // namespace

void RunMewUiFurnitureMoveProbeTests() {
    auto root = std::make_unique<std::array<
        std::uint8_t,
        AC_MEW_FURNITURE_PROBE_NODE_BYTES>>();
    auto child = std::make_unique<std::array<
        std::uint8_t,
        AC_MEW_FURNITURE_PROBE_NODE_BYTES>>();
    root->fill(0);
    child->fill(0);

    const auto child_address = reinterpret_cast<std::uint64_t>(child->data());
    const auto vtable_like = reinterpret_cast<std::uint64_t>(
        &ProbeVtableAnchor);
    std::memcpy(root->data() + 0x20U, &child_address, sizeof(child_address));
    std::memcpy(child->data(), &vtable_like, sizeof(vtable_like));
    const std::int32_t before_x = -6;
    const double before_world_x = 12.5;
    std::memcpy(child->data() + 0x58U, &before_x, sizeof(before_x));
    std::memcpy(
        child->data() + 0xA0U,
        &before_world_x,
        sizeof(before_world_x));

    auto before = std::make_unique<AcMewFurnitureMoveSample>();
    auto after = std::make_unique<AcMewFurnitureMoveSample>();
    AC_CHECK(AcMewCaptureFurnitureMoveSample(root->data(), before.get()));
    AC_CHECK(before->node_count >= 2U);

    const std::int32_t after_x = 3;
    const double after_world_x = 21.5;
    std::memcpy(child->data() + 0x58U, &after_x, sizeof(after_x));
    std::memcpy(
        child->data() + 0xA0U,
        &after_world_x,
        sizeof(after_world_x));
    AC_CHECK(AcMewCaptureFurnitureMoveSample(root->data(), after.get()));

    auto difference = std::make_unique<AcMewFurnitureMoveDiff>();
    AcMewCompareFurnitureMoveSamples(
        before.get(),
        after.get(),
        AC_MEW_FURNITURE_PROBE_UI,
        difference.get());
    AC_CHECK(difference->changed_node_count >= 1U);
    const auto* child_difference = FindChangedNode(
        *difference,
        child_address);
    AC_CHECK(child_difference != nullptr);
    AC_CHECK(child_difference->changed_bytes != 0U);
    AC_CHECK(child_difference->before_vtable_rva != 0U);
    AC_CHECK(child_difference->before_vtable_rva ==
             child_difference->after_vtable_rva);

    bool found_x{};
    for (std::uint32_t index = 0;
         index < child_difference->int32_candidate_count;
         ++index) {
        const auto& candidate = child_difference->int32_candidates[index];
        found_x = found_x ||
            (candidate.offset == 0x58U &&
             candidate.before == before_x &&
             candidate.after == after_x);
    }
    AC_CHECK(found_x);

    bool found_world_x{};
    for (std::uint32_t index = 0;
         index < child_difference->double_candidate_count;
         ++index) {
        const auto& candidate = child_difference->double_candidates[index];
        found_world_x = found_world_x ||
            (candidate.offset == 0xA0U &&
             candidate.before == before_world_x &&
             candidate.after == after_world_x);
    }
    AC_CHECK(found_world_x);

    auto report = std::make_unique<ui::FurnitureMoveProbeReport>();
    report->scene_generation = 17;
    report->furniture_ui = *difference;
    AcMewCompareFurnitureMoveSamples(
        before.get(),
        before.get(),
        AC_MEW_FURNITURE_PROBE_HOUSE_SCENE,
        &report->house_scene);
    const auto directory = std::filesystem::temp_directory_path() /
        ("auto-cattery-furniture-probe-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto written = ui::WriteFurnitureMoveProbeReport(
        directory,
        *report);
    AC_CHECK(static_cast<bool>(written));
    AC_CHECK(std::filesystem::exists(written.value));
    std::ifstream input(written.value, std::ios::binary);
    const std::string text{
        std::istreambuf_iterator<char>(input), {}};
    AC_CHECK(text.find("furniture_manual_move_object_graph_delta") !=
             std::string::npos);
    AC_CHECK(text.find("FurnitureBuildingUI") != std::string::npos);
    AC_CHECK(text.find("\"offset\": 88") != std::string::npos);
    input.close();
    std::filesystem::remove_all(directory);
}

}  // namespace autocattery::tests
