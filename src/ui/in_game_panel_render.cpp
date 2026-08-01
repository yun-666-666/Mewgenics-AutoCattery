#include "in_game_panel_controller.hpp"

#include <algorithm>

namespace autocattery::ui {
namespace {

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
    auto label = cat.display_name + "  |  ID " + std::to_string(cat.cat_id);
    label += "  |  房间 " + cat.room_id.value_or("房外");
    if (cat.level) label += "  |  " + std::string(LevelName(*cat.level));
    if (cat.fixed_room) label += "  |  固定 " + *cat.fixed_room;
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
    const auto saves = protection_->saves();
    content.rows.push_back(
        "<  存档：" + saves[protection_->selected_save()].label + "  >");
    const auto cats = protection_->cats();
    for (std::size_t visible = 0; visible < 5; ++visible) {
        const auto index = cat_page_ * 5 + visible;
        content.rows.push_back(
            index < cats.size() ? CatLabel(cats[index]) : "—");
    }
    content.rows.push_back(
        "<  保护等级：" + std::string(LevelName(selected_level_)) + "  >");
    content.rows.push_back(
        "<  固定房间：" + selected_room_.value_or("不固定") + "  >");
    if (selected_cat_) {
        const auto first = cat_page_ * 5;
        if (*selected_cat_ >= first && *selected_cat_ < first + 5) {
            content.selected_row = 1 + *selected_cat_ - first;
        }
        content.show_apply = true;
        content.show_remove = cats[*selected_cat_].level.has_value() ||
            cats[*selected_cat_].fixed_room.has_value();
    }
    const auto pages = std::max<std::size_t>(1, (cats.size() + 4) / 5);
    content.title += " · 猫列表 " + std::to_string(cat_page_ + 1) +
        "/" + std::to_string(pages);
    return content;
}

}  // namespace autocattery::ui
