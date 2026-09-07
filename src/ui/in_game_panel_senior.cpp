#include "in_game_panel_controller.hpp"

#include <algorithm>
#include <limits>
#include <tuple>

#include "mew_ui_house_cat_probe.h"
#include "mew_ui_house_detail_adapter.h"

namespace autocattery::ui {
namespace {

std::int64_t BaseTotal(const snapshot::CatSnapshot& cat) {
    std::int64_t total{};
    for (const auto value : cat.genetic_stats.values) {
        if (!value) return std::numeric_limits<std::int64_t>::min();
        total += *value;
    }
    return total;
}

}  // namespace

std::vector<const snapshot::CatSnapshot*>
InGamePanelController::SeniorCats() const {
    std::vector<const snapshot::CatSnapshot*> cats;
    if (protection_loading_ || !protection_ || !current_save_) return cats;
    for (const auto& cat : protection_->saves()[*current_save_].snapshot.cats) {
        if (cat.life_stage == snapshot::LifeStage::Senior) cats.push_back(&cat);
    }
    std::ranges::sort(cats, [this](const auto* a, const auto* b) {
        const auto age_a = a->age_days.value_or(-1);
        const auto age_b = b->age_days.value_or(-1);
        const auto key_a = senior_sort_by_stats_
            ? std::tuple{BaseTotal(*a), age_a} : std::tuple{age_a, BaseTotal(*a)};
        const auto key_b = senior_sort_by_stats_
            ? std::tuple{BaseTotal(*b), age_b} : std::tuple{age_b, BaseTotal(*b)};
        return key_a != key_b ? key_a > key_b : a->id < b->id;
    });
    return cats;
}

ManagementPanelContent InGamePanelController::SeniorContent() {
    ManagementPanelContent content;
    content.page = ManagementPanelPage::Senior;
    content.title = English() ? "Senior Cats" : "年迈猫";
    if (protection_loading_ || !current_save_) {
        content.rows = {protection_loading_
            ? (English() ? "Reading current save..." : "正在读取当前存档…")
            : (English() ? "Current save unconfirmed; click to refresh"
                         : "当前存档未确认；点击此处重新读取")};
        content.status = status_;
        return content;
    }
    const auto cats = SeniorCats();
    const auto pages = std::max<std::size_t>(1, (cats.size() + 8) / 9);
    senior_page_ = std::min(senior_page_, pages - 1);
    content.title += " " + std::to_string(cats.size()) + " | " +
        std::to_string(senior_page_ + 1) + "/" + std::to_string(pages);
    content.rows = {English() ? "Current save · Click to refresh"
                             : "当前存档 · 点击刷新"};
    for (std::size_t row = 0; row < 9; ++row) {
        const auto index = senior_page_ * 9 + row;
        if (index >= cats.size()) {
            content.rows.emplace_back();
            continue;
        }
        const auto& cat = *cats[index];
        auto text = cat.display_name + " #" + std::to_string(cat.id) + "\n";
        text += (English() ? "Age " : "年龄 ") +
            (cat.age_days ? std::to_string(*cat.age_days) : "?") + " | ";
        text += (English() ? "Base " : "基础 ");
        for (std::size_t i = 0; i < snapshot::kStatCount; ++i) {
            if (i != 0) text += "/";
            const auto value = cat.genetic_stats.values[i];
            text += value ? std::to_string(*value) : "?";
        }
        content.rows.push_back(std::move(text));
    }
    content.rows.push_back(senior_sort_by_stats_
        ? (English() ? "Sort: base total ↓ (click to switch)" : "排序：基础总值优先（点击切换）")
        : (English() ? "Sort: age ↓ (click to switch)" : "排序：年龄优先（点击切换）"));
    content.status = cats.empty()
        ? (English() ? "No living cats marked old in this saved state."
                     : "当前已保存状态中没有被游戏标记为“老了”的活猫。")
        : (English() ? "STR/DEX/CON/INT/SPD/CHA/LCK. Click a cat for details."
                     : "属性顺序：力/敏/体/智/速/魅/运；点击猫打开详情。未保存变化需保存后刷新。");
    content.show_navigation = pages > 1;
    return content;
}

void InGamePanelController::HandleSeniorRow(std::size_t row) {
    if (row == 0) {
        if (!protection_loading_) StartProtectionLoad();
        return;
    }
    if (row == 10) {
        senior_sort_by_stats_ = !senior_sort_by_stats_;
        senior_page_ = 0;
        return;
    }
    const auto cats = SeniorCats();
    const auto index = senior_page_ * 9 + row - 1;
    if (row > 9 || index >= cats.size() || !current_save_) return;
    const auto& house = protection_->saves()[*current_save_].snapshot;
    std::vector<snapshot::CatId> ids;
    for (const auto& cat : house.cats) ids.push_back(cat.id);
    std::vector<AcMewHouseCatMatch> matches(ids.size());
    const auto result = AcMewProbeHouseCatIdentity(house_scene_manager_,
        ids.data(), ids.size(), matches.data(), matches.size());
    if (!result.stable_bijection || !result.consistent_mapping ||
        result.match_count != ids.size()) {
        status_ = English() ? "Cats changed; refresh before opening details"
                            : "猫群已变化，请刷新后再打开详情";
        StartProtectionLoad();
        return;
    }
    const auto found = std::ranges::find(matches, cats[index]->id,
                                        &AcMewHouseCatMatch::cat_id);
    if (found == matches.end()) return;
    const auto opened = AcMewOpenHouseCatDetails(house_scene_manager_, found->component);
    if (opened.invoked) Close();
    else status_ = English() ? "Cat details unavailable" : "暂时无法打开猫详情";
}

}  // namespace autocattery::ui
