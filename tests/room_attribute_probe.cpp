#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"

#include <filesystem>
#include <iostream>
#include <unordered_map>

int wmain(int argument_count, wchar_t** arguments) {
    if (argument_count != 3) {
        std::cerr << "usage: room_attribute_probe <save> <game-root>\n";
        return 2;
    }
    autocattery::snapshot::SaveSnapshotAdapter adapter{
        std::filesystem::path(arguments[1]),
        std::filesystem::path(arguments[2])};
    const auto captured = adapter.CaptureHouseSnapshot(1);
    if (!captured) {
        std::cerr << captured.message << '\n';
        return 1;
    }
    std::cout << "save=" << captured.value.source_save_name
              << " attributes="
              << captured.value.capabilities.read_room_attributes
              << '\n';
    for (const auto& room : captured.value.rooms) {
        if (!room.attributes) {
            std::cout << room.id << " unavailable\n";
            continue;
        }
        std::cout << room.id
                  << " cats=" << room.residents.size()
                  << " comfort=" << room.attributes->comfort
                  << " stimulation=" << room.attributes->stimulation
                  << " health=" << room.attributes->health
                  << " mutation=" << room.attributes->mutation
                  << " appeal=" << room.attributes->appeal << '\n';
    }
    autocattery::workflow::WorkflowStateMachine state;
    if (!state.BeginPreview()) {
        return 1;
    }
    const auto preview = autocattery::workflow::PreviewBuilder(adapter).Build(
        2, autocattery::workflow::WorkflowCapability::MoveOnly, state);
    if (!preview) {
        std::cerr << preview.message << '\n';
        return 1;
    }
    std::unordered_map<autocattery::snapshot::CatId, std::string> targets;
    for (const auto& cat : preview.value.snapshot.cats) {
        targets.emplace(cat.id, cat.room_id.value_or("Outside"));
    }
    for (const auto& move : preview.value.room_plan.moves) {
        targets.at(move.cat_id) = move.to_room;
    }
    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& [cat_id, room] : targets) {
        (void)cat_id;
        ++counts[room];
    }
    std::cout << "moves=" << preview.value.room_plan.moves.size();
    for (const auto& [room, count] : counts) {
        std::cout << ' ' << room << '=' << count;
    }
    std::cout << '\n';
    return 0;
}
