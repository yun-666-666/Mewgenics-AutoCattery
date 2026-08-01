#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "auto_cattery/protection/sidecar.hpp"

namespace autocattery::protection {

struct ProtectionSaveOption {
    std::string label;
    snapshot::HouseSnapshot snapshot;
};

struct ProtectionCatOption {
    snapshot::CatId cat_id{};
    std::string display_name;
    std::optional<snapshot::RoomId> room_id;
    std::string identity_token;
    std::optional<ProtectionLevel> level;
    std::optional<snapshot::RoomId> fixed_room;
};

class ProtectionEditorModel final {
public:
    ProtectionEditorModel(
        std::filesystem::path sidecar_path,
        std::filesystem::path game_root,
        std::filesystem::path save_root = {});
    ProtectionEditorModel(
        std::filesystem::path sidecar_path,
        std::vector<snapshot::HouseSnapshot> snapshots);

    [[nodiscard]] Result<void> Reload();
    [[nodiscard]] Result<void> SelectSave(std::size_t index);
    [[nodiscard]] Result<void> Apply(
        std::size_t cat_index,
        ProtectionLevel level,
        std::optional<snapshot::RoomId> fixed_room);
    [[nodiscard]] Result<void> Remove(std::size_t cat_index);

    [[nodiscard]] std::span<const ProtectionSaveOption> saves() const;
    [[nodiscard]] std::span<const ProtectionCatOption> cats() const;
    [[nodiscard]] std::span<const snapshot::RoomId> rooms() const;
    [[nodiscard]] std::size_t selected_save() const noexcept;

private:
    void RebuildSelectedOptions();

    std::filesystem::path sidecar_path_;
    std::filesystem::path game_root_;
    std::filesystem::path save_root_;
    ProtectionSidecar sidecar_;
    std::vector<ProtectionSaveOption> saves_;
    std::vector<snapshot::HouseSnapshot> preset_snapshots_;
    std::vector<ProtectionCatOption> cats_;
    std::vector<snapshot::RoomId> rooms_;
    std::size_t selected_save_{};
};

}  // namespace autocattery::protection
