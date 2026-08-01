#include "in_game_panel_controller.hpp"

#include <array>
#include <chrono>

#include "auto_cattery/settings_file_editor.hpp"

namespace autocattery::ui {
namespace {

constexpr std::array kLevels{
    protection::ProtectionLevel::NoCull,
    protection::ProtectionLevel::NoMove,
    protection::ProtectionLevel::NoCullOrMove,
    protection::ProtectionLevel::FullyUnmanaged
};

const char* LevelName(protection::ProtectionLevel level) {
    using enum protection::ProtectionLevel;
    switch (level) {
    case NoCull: return "禁止淘汰";
    case NoMove: return "禁止移动";
    case NoCullOrMove: return "禁止淘汰和移动";
    case FullyUnmanaged: return "完全不管理";
    default: return "无";
    }
}

template<class T>
std::size_t Cycle(std::size_t current, std::size_t count, T direction) {
    if (count == 0) return 0;
    if (direction < 0) return current == 0 ? count - 1 : current - 1;
    return (current + 1) % count;
}

}  // namespace

void InGamePanelController::StartProtectionLoad() {
    protection_loading_ = true;
    current_save_.reset();
    current_save_checked_ = false;
    status_ = "正在读取本机存档和猫身份，请稍候…";
    const auto mod_root = mod_root_;
    const auto game_root = game_root_;
    protection_task_ = std::async(
        std::launch::async,
        [mod_root, game_root]() -> ProtectionLoad {
            SettingsFileEditor config_editor({
                mod_root / L"config" / L"default_config.json",
                mod_root / L"config" / L"user_config.json"});
            const auto config = config_editor.Load();
            if (!config) return {{}, config.code, config.message};
            auto model = std::make_shared<ProtectionModel>(
                mod_root / L"config" /
                    std::filesystem::path(config.value.protection.sidecar_file),
                game_root);
            const auto loaded = model->Reload();
            if (!loaded) return {{}, loaded.code, loaded.message};
            return {std::move(model)};
        });
}

void InGamePanelController::PollProtectionLoad() {
    if (!protection_loading_ || !protection_task_.valid() ||
        protection_task_.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) return;
    auto loaded = protection_task_.get();
    protection_loading_ = false;
    if (!loaded) {
        status_ = "保护数据读取失败：" + loaded.message;
    } else {
        protection_ = std::move(loaded.value);
        ResolveCurrentSave();
        cat_page_ = 0;
        selected_cat_.reset();
        status_ = "先明确选择一只猫，再设置保护等级或固定房间";
    }
    if (open_ && protection_page_) Render();
}

void InGamePanelController::HandleProtectionRow(
    std::size_t row, int direction) {
    if (!protection_) {
        status_ = protection_loading_ ? "保护数据仍在读取"
                                      : "保护数据不可用";
        return;
    }
    if (protection_choice_ != ProtectionChoice::None) {
        if (row == 0) {
            protection_choice_ = ProtectionChoice::None;
            choice_page_ = 0;
            status_ = "已返回当前存档猫列表";
            return;
        }
        if (row < 1 || row > 9) return;
        const auto index = choice_page_ * 9 + row - 1;
        if (protection_choice_ == ProtectionChoice::Level) {
            if (index >= kLevels.size()) return;
            selected_level_ = kLevels[index];
            status_ = "已选择保护等级：" +
                std::string(LevelName(selected_level_));
        } else {
            const auto rooms = protection_->rooms();
            if (index > rooms.size()) return;
            selected_room_ = index == 0 ? std::nullopt
                : std::optional(rooms[index - 1]);
            status_ = "已选择固定房间：" +
                selected_room_.value_or("不固定");
        }
        protection_choice_ = ProtectionChoice::None;
        choice_page_ = 0;
        return;
    }
    if (row == 0) {
        const auto count = protection_->saves().size();
        const auto next = Cycle(
            protection_->selected_save(), count, direction);
        const auto selected = protection_->SelectSave(next);
        status_ = selected ? "已切换存档；请重新选择猫"
                           : "切换失败：" + selected.message;
        cat_page_ = 0;
        selected_cat_.reset();
        selected_room_.reset();
        protection_choice_ = ProtectionChoice::None;
        return;
    }
    if (row >= 1 && row <= 9) {
        const auto index = cat_page_ * 9 + row - 1;
        if (index >= protection_->cats().size()) return;
        selected_cat_ = index;
        const auto& cat = protection_->cats()[index];
        selected_level_ = cat.level.value_or(
            protection::ProtectionLevel::NoMove);
        selected_room_ = cat.fixed_room;
        status_ = "已选择 " + cat.display_name + "；现在可以应用或移除";
        return;
    }
    if (!selected_cat_) {
        status_ = "请先明确选择一只猫";
        return;
    }
    if (row == 10) {
        protection_choice_ = ProtectionChoice::Level;
        choice_page_ = 0;
        status_ = "请选择保护等级";
    } else if (row == 11) {
        protection_choice_ = ProtectionChoice::Room;
        choice_page_ = 0;
        status_ = "请选择固定房间";
    } else if (row == 12) {
        const auto applied = protection_->Apply(
            *selected_cat_, selected_level_, selected_room_);
        status_ = applied ? "保护规则已保存"
                          : "保护保存失败：" + applied.message;
    } else if (row == 13) {
        const auto removed = protection_->Remove(*selected_cat_);
        status_ = removed ? "该猫的玩家保护规则已移除"
                          : "移除失败：" + removed.message;
    }
}

}  // namespace autocattery::ui
