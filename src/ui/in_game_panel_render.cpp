#include "in_game_panel_controller.hpp"

#include <algorithm>
#include <array>

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

std::string CatLabel(const protection::ProtectionCatOption& cat) {
    auto label = cat.display_name + " #" + std::to_string(cat.cat_id);
    label += " | " + cat.room_id.value_or("房外");
    if (cat.level) label += " | " + std::string(LevelName(*cat.level));
    if (cat.fixed_room) label += " | 固定:" + *cat.fixed_room;
    return label;
}

}  // namespace

ManagementPanelContent InGamePanelController::ProtectionContent() {
    ManagementPanelContent content;
    content.protection_page = true;
    content.title = "猫保护";
    content.status = status_;
    if (protection_loading_) {
        content.rows = {"正在读取本机存档…"};
        return content;
    }
    if (!protection_ || protection_->saves().empty()) {
        content.rows = {"没有可读取的存档；外部保护编辑器仍可使用"};
        return content;
    }
    if (protection_choice_ != ProtectionChoice::None) {
        content.title = protection_choice_ == ProtectionChoice::Level
            ? "猫保护 · 选择保护等级" : "猫保护 · 选择固定房间";
        content.rows.push_back("返回当前存档猫列表");
        std::vector<std::string> choices;
        std::size_t selected{};
        if (protection_choice_ == ProtectionChoice::Level) {
            for (const auto level : kLevels) choices.emplace_back(LevelName(level));
            const auto found = std::find(kLevels.begin(), kLevels.end(), selected_level_);
            if (found != kLevels.end()) selected = found - kLevels.begin();
        } else {
            choices.emplace_back("不固定");
            const auto rooms = protection_->rooms();
            choices.insert(choices.end(), rooms.begin(), rooms.end());
            if (selected_room_) {
                const auto found = std::find(rooms.begin(), rooms.end(), *selected_room_);
                if (found != rooms.end()) selected = 1 + (found - rooms.begin());
            }
        }
        const auto first = choice_page_ * 9;
        for (std::size_t visible = 0; visible < 9; ++visible) {
            const auto index = first + visible;
            content.rows.push_back(index < choices.size() ? choices[index] : "");
        }
        if (selected >= first && selected < first + 9)
            content.selected_row = 1 + selected - first;
        const auto pages = std::max<std::size_t>(1, (choices.size() + 8) / 9);
        content.show_navigation = pages > 1;
        content.status = "点击一个选项；点击顶部返回栏可取消";
        return content;
    }
    const auto saves = protection_->saves();
    auto save_label = saves[protection_->selected_save()].label;
    if (current_save_) {
        save_label += protection_->selected_save() == *current_save_
            ? " | 当前存档" : " | 非当前存档";
    } else if (current_save_checked_) {
        save_label += " | 当前状态未确认";
    }
    content.rows.push_back(
        "<  存档：" + save_label + "  >");
    const auto cats = protection_->cats();
    for (std::size_t visible = 0; visible < 9; ++visible) {
        const auto index = cat_page_ * 9 + visible;
        content.rows.push_back(
            index < cats.size() ? CatLabel(cats[index]) : "");
    }
    content.rows.push_back(
        "<  保护等级：" + std::string(LevelName(selected_level_)) + "  >");
    content.rows.push_back(
        "<  固定房间：" + selected_room_.value_or("不固定") + "  >");
    if (selected_cat_) {
        const auto first = cat_page_ * 9;
        if (*selected_cat_ >= first && *selected_cat_ < first + 9) {
            content.selected_row = 1 + *selected_cat_ - first;
        }
        content.show_apply = true;
        content.show_remove = cats[*selected_cat_].level.has_value() ||
            cats[*selected_cat_].fixed_room.has_value();
    }
    const auto pages = std::max<std::size_t>(1, (cats.size() + 8) / 9);
    content.show_navigation = pages > 1;
    content.title += " · 猫列表 " + std::to_string(cat_page_ + 1) +
        "/" + std::to_string(pages);
    return content;
}

}  // namespace autocattery::ui
