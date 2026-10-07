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
InGamePanelController::SeniorCats() {
    std::vector<const snapshot::CatSnapshot*> cats;
    if (protection_loading_ || !protection_ || !current_save_) return cats;
    const auto& snapshot = protection_->saves()[*current_save_].snapshot;
    if (delivery_preview_ || population_preview_) {
        if (!protection_->SelectSave(*current_save_)) return cats;
        std::vector<snapshot::CatId> ids;
        if (population_preview_) {
            if (!population_plan_) population_plan_ = breeding::SelectPopulation(
                snapshot, protection_->cats(), settings_.CurrentConfig());
            if (!*population_plan_) { status_ = population_plan_->message; return cats; }
            ids = population_plan_->value.surplus;
        } else ids = PlanDeadCatDelivery(snapshot, protection_->cats());
        for (const auto id : ids) {
            const auto cat = std::ranges::find(snapshot.cats, id, &snapshot::CatSnapshot::id);
            cats.push_back(&*cat);
        }
        return cats;
    }
    for (const auto& cat : snapshot.cats) {
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
    content.title = population_preview_
        ? (English() ? "Surplus Cat Preview" : "超额猫淘汰预览")
        : delivery_preview_
        ? (English() ? "Dead Cat Delivery Preview" : "死亡猫交付预览")
        : (English() ? "Senior Cats" : "年迈猫");
    if (protection_loading_ || !current_save_) {
        content.rows = {protection_loading_
            ? (English() ? "Reading current save..." : "正在读取当前存档…")
            : (English() ? "Current save unconfirmed; click to refresh"
                         : "当前存档未确认；点击此处重新读取")};
        content.status = delivery_preview_ && !delivery_.Message().empty()
            ? delivery_.Message() : status_;
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
    content.rows.push_back(population_preview_
        ? (English() ? "Confirm: NPCs first, trash if none accepts" : "确认：超额猫优先送NPC，无人接收才送垃圾桶")
        : delivery_preview_
        ? (English() ? "Confirm: deliver to Organ Grinder" : "确认按编号顺序交付给接收死猫的NPC")
        : senior_sort_by_stats_
        ? (English() ? "Sort: base total ↓ (click to switch)" : "排序：基础总值优先（点击切换）")
        : (English() ? "Sort: age ↓ (click to switch)" : "排序：年龄优先（点击切换）"));
    content.rows.push_back(population_preview_
        ? (English() ? "Back to senior cats" : "返回年迈猫")
        : delivery_preview_
        ? (English() ? "Preview surplus cats" : "超额猫淘汰：查看预览")
        : (English() ? "Preview dead cat delivery" : "死亡猫交付：查看预览"));
    content.status = cats.empty()
        ? (English() ? "No living cats marked old in this saved state."
                     : "当前已保存状态中没有被游戏标记为“老了”的活猫。")
        : (English() ? "STR/DEX/CON/INT/SPD/CHA/LCK. Click a cat for details."
                     : "属性顺序：力/敏/体/智/速/魅/运；点击猫打开详情。未保存变化需保存后刷新。");
    if (delivery_preview_ || population_preview_) {
        content.status = "先正常保存；不生成备份。Esc/F10停止后续交付；需撤销请勿保存并重新读档。";
        if (population_preview_ && population_plan_) {
            if (!*population_plan_) content.status = population_plan_->message;
            else {
                content.title += " | " + std::to_string(population_plan_->value.retained.size()) +
                    "/" + std::to_string(settings_.CurrentConfig().room_planning.population_limit);
                if (!population_plan_->value.limit_reached)
                    content.status = "保护猫数量超过上限，保留全部保护猫；请调整上限或保护设置。";
            }
        }
        if (!delivery_.Message().empty()) content.status = delivery_.Message();
        if (population_preview_ && !population_error_.empty()) content.status = population_error_;
        if (population_auto_preview_) {
            content.title += " | " + std::to_string(population_countdown_) +
                (English() ? "s until auto delivery" : "秒后自动淘汰");
            content.status = English()
                ? "Esc/F10/Close cancels this round. To undo delivery, reload without saving."
                : "Esc/F10/关闭取消本轮；执行中也可停止后续交付。需撤销请勿保存并重新读档。";
        }
        if (population_preview_ && population_error_.empty())
            content.status += English()
                ? " NPCs first; reserve unrelated founder bloodlines within the cap."
                : " 优先接收NPC；上限内预留未留下后代的独立血线。";
    }
    content.show_navigation = pages > 1;
    return content;
}

void InGamePanelController::HandleSeniorRow(std::size_t row) {
    if (row == 0) {
        if (!protection_loading_) StartProtectionLoad();
        return;
    }
    if (row == 11) {
        if (population_preview_) population_preview_ = false;
        else if (delivery_preview_) { delivery_preview_ = false; population_preview_ = true; }
        else delivery_preview_ = true;
        population_plan_.reset();
        senior_page_ = 0;
        if (!protection_loading_) StartProtectionLoad();
        return;
    }
    if (row == 10 && (delivery_preview_ || population_preview_)) {
        StartDeadCatDelivery();
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

void InGamePanelController::StartDeadCatDelivery() {
    if (!current_save_ || !protection_ || protection_loading_) return;
    ResolveCurrentSave();
    if (!current_save_) return;
    const auto cats = SeniorCats();
    std::vector<snapshot::CatId> ids;
    for (const auto* cat : cats) ids.push_back(cat->id);
    const auto& save = protection_->saves()[*current_save_];
    if (population_preview_) {
        const auto& config = settings_.CurrentConfig();
        if (!config.mod_enabled || config.safe_mode || config.force_read_only ||
            config.execution_safety.read_only_mode) {
            population_error_ = "当前为安全/只读模式，不能淘汰猫";
            return;
        }
    }
    const auto started = delivery_.Start(save.snapshot, std::move(ids), save.path, mod_root_, game_root_,
        population_preview_ ? DeadCatDeliveryService::Recipient::NpcPreferred :
            DeadCatDeliveryService::Recipient::OrganGrinder);
    if (!started) { status_ = started.message; return; }
    Close();
    view_.StartDeliveryInput();
}
}
