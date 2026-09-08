#include "auto_cattery/protection/editor_model.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

#include "auto_cattery/protection/identity.hpp"
#include "auto_cattery/protection/sidecar.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

namespace autocattery::protection {
namespace {

ProtectionSidecar EmptySidecar() {
    return ParseProtectionSidecar(
        R"({"schema_version":1,"records":[],"blacklist":[]})");
}

const SidecarRecord* FindRecord(
    const ProtectionSidecar& sidecar,
    std::string_view identity_token) {
    const auto found = std::ranges::find(
        sidecar.records, identity_token,
        &SidecarRecord::identity_token);
    return found == sidecar.records.end() ? nullptr : &*found;
}

std::string SaveLabel(
    const snapshot::HouseSnapshot& snapshot,
    std::size_t ordinal) {
    return snapshot.source_save_name + " | " +
        std::to_string(snapshot.cats.size()) + " cats | #" +
        std::to_string(ordinal + 1);
}

}  // namespace

ProtectionEditorModel::ProtectionEditorModel(
    std::filesystem::path sidecar_path,
    std::filesystem::path game_root,
    std::filesystem::path save_root)
    : sidecar_path_(std::move(sidecar_path)),
      game_root_(std::move(game_root)),
      save_root_(std::move(save_root)) {}

ProtectionEditorModel::ProtectionEditorModel(
    std::filesystem::path sidecar_path,
    std::vector<snapshot::HouseSnapshot> snapshots)
    : sidecar_path_(std::move(sidecar_path)),
      preset_snapshots_(std::move(snapshots)) {}

Result<void> ProtectionEditorModel::Reload() {
    auto loaded = LoadProtectionSidecar(sidecar_path_);
    if (loaded.status == SidecarLoadStatus::Missing) {
        loaded = EmptySidecar();
    }
    if (loaded.status != SidecarLoadStatus::Loaded) {
        return {ErrorCode::ConfigInvalid,
                "protection.json is invalid: " + loaded.limitation};
    }

    std::vector<ProtectionSaveOption> saves;
    std::string discovery_error;
    if (!preset_snapshots_.empty()) {
        for (auto snapshot : preset_snapshots_) {
            saves.push_back({
                SaveLabel(snapshot, saves.size()), std::move(snapshot)
            });
        }
    } else {
        const auto paths = snapshot::detail::FindSaveCandidates(
            save_root_, discovery_error);
        for (const auto& path : paths) {
            snapshot::SaveSnapshotAdapter adapter(path, game_root_);
            auto captured = adapter.CaptureHouseSnapshot(1);
            if (!captured) {
                continue;
            }
            saves.push_back({
                SaveLabel(captured.value, saves.size()),
                std::move(captured.value), path
            });
        }
    }
    if (saves.empty()) {
        return {ErrorCode::CatDataUnavailable,
                "no readable Mewgenics save: " + discovery_error};
    }
    sidecar_ = std::move(loaded);
    saves_ = std::move(saves);
    selected_save_ = 0;
    RebuildSelectedOptions();
    return {};
}

Result<void> ProtectionEditorModel::SelectSave(std::size_t index) {
    if (index >= saves_.size()) {
        return {ErrorCode::ConfigInvalid, "save selection is invalid"};
    }
    selected_save_ = index;
    RebuildSelectedOptions();
    return {};
}

Result<void> ProtectionEditorModel::Apply(
    std::size_t cat_index,
    ProtectionLevel level,
    std::optional<snapshot::RoomId> fixed_room) {
    if (cat_index >= cats_.size() || level == ProtectionLevel::None) {
        return {ErrorCode::ConfigInvalid, "cat protection input is invalid"};
    }
    if (fixed_room &&
        std::ranges::find(rooms_, *fixed_room) == rooms_.end()) {
        return {ErrorCode::ConfigInvalid, "fixed room is unavailable"};
    }
    const auto& cat = cats_[cat_index];
    SidecarRecord record;
    record.protection.cat_id = cat.cat_id;
    record.protection.level = level;
    record.protection.reason = "player protection editor";
    record.identity_token = cat.identity_token;
    record.display_name = cat.display_name;
    record.fixed_room = std::move(fixed_room);
    auto candidate = sidecar_;
    UpsertProtectionRecord(candidate, std::move(record));
    const auto saved = SaveProtectionSidecar(sidecar_path_, candidate);
    if (!saved) {
        return saved;
    }
    sidecar_ = std::move(candidate);
    RebuildSelectedOptions();
    return {};
}

Result<void> ProtectionEditorModel::Remove(std::size_t cat_index) {
    if (cat_index >= cats_.size()) {
        return {ErrorCode::ConfigInvalid, "cat selection is invalid"};
    }
    auto candidate = sidecar_;
    RemoveProtectionRecord(candidate, cats_[cat_index].identity_token);
    const auto saved = SaveProtectionSidecar(sidecar_path_, candidate);
    if (!saved) {
        return saved;
    }
    sidecar_ = std::move(candidate);
    RebuildSelectedOptions();
    return {};
}

void ProtectionEditorModel::RebuildSelectedOptions() {
    cats_.clear();
    rooms_.clear();
    if (saves_.empty()) {
        return;
    }
    const auto& snapshot = saves_[selected_save_].snapshot;
    for (const auto& room : snapshot.rooms) {
        rooms_.push_back(room.id);
    }
    std::sort(rooms_.begin(), rooms_.end());
    for (const auto& cat : snapshot.cats) {
        ProtectionCatOption option{
            cat.id, cat.display_name, cat.room_id,
            StableCatIdentityToken(cat), std::nullopt, std::nullopt
        };
        if (const auto* record = FindRecord(
                sidecar_, option.identity_token)) {
            option.level = record->protection.level;
            option.fixed_room = record->fixed_room;
        }
        cats_.push_back(std::move(option));
    }
    std::sort(
        cats_.begin(), cats_.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.display_name, left.cat_id) <
                std::tie(right.display_name, right.cat_id);
        });
}

std::span<const ProtectionSaveOption>
ProtectionEditorModel::saves() const { return saves_; }
std::span<const ProtectionCatOption>
ProtectionEditorModel::cats() const { return cats_; }
std::span<const snapshot::RoomId>
ProtectionEditorModel::rooms() const { return rooms_; }
std::size_t ProtectionEditorModel::selected_save() const noexcept {
    return selected_save_;
}

}  // namespace autocattery::protection
