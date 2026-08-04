#include "in_game_settings_model.hpp"

#include <algorithm>

namespace autocattery::ui {

namespace {

const char* L(bool english, const char* chinese, const char* translated) {
    return english ? translated : chinese;
}

}  // namespace

std::vector<InGameSettingsModel::Page> InGameSettingsModel::Pages() {
    using F = Field;
    auto& combat = config_.combat_scoring;
    auto& breeding = config_.breeding_scoring;
    auto& classes = config_.classification;
    auto& rooms = config_.room_planning;
    auto& marker = config_.recommendation_marker;
    auto& safety = config_.execution_safety;
    auto& general = config_.general;
    auto& diagnostics = config_.diagnostics;
    auto& level_up = config_.level_up;
    const bool en = IsEnglish();
    std::vector<std::pair<const char*, std::vector<F>>> groups;
    groups.emplace_back(L(en, "战斗评分", "Combat Scoring"), std::vector<F>{
        {L(en, "推荐猫数量", "Recommended cats"), &combat.recommended_count, 1, 100, 1},
        {L(en, "最低战斗分数", "Minimum combat score"), &combat.minimum_score, -10000, 10000, 1},
        {L(en, "最少已知属性", "Minimum known stats"), &combat.minimum_known_stats, 0, 7, 1},
        {L(en, "排除幼猫", "Exclude kittens"), &combat.exclude_kittens},
        {L(en, "排除受伤猫", "Exclude injured cats"), &combat.exclude_injured},
        {L(en, "要求资格已确认", "Require confirmed eligibility"), &combat.require_confirmed_eligibility},
        {L(en, "缺失属性惩罚", "Missing stat penalty"), &combat.missing_stat_penalty, -10000, 10000, .25},
        {L(en, "受伤惩罚", "Injury penalty"), &combat.injury_penalty, -10000, 10000, .25},
        {L(en, "力量权重", "Strength weight"), &combat.stat_weights[0], -10000, 10000, .25},
        {L(en, "敏捷权重", "Dexterity weight"), &combat.stat_weights[1], -10000, 10000, .25},
        {L(en, "体质权重", "Constitution weight"), &combat.stat_weights[2], -10000, 10000, .25},
        {L(en, "智力权重", "Intelligence weight"), &combat.stat_weights[3], -10000, 10000, .25},
        {L(en, "速度权重", "Speed weight"), &combat.stat_weights[4], -10000, 10000, .25},
        {L(en, "魅力权重", "Charisma weight"), &combat.stat_weights[5], -10000, 10000, .25},
        {L(en, "幸运权重", "Luck weight"), &combat.stat_weights[6], -10000, 10000, .25},
        {L(en, "推荐显示分数", "Show recommendation score"), &marker.show_score},
        {L(en, "推荐显示排名", "Show recommendation rank"), &marker.show_rank},
    });
    groups.emplace_back(L(en, "繁育与分类", "Breeding & Classification"), std::vector<F>{
        {L(en, "核心繁育猫数", "Core breeders"), &breeding.core_breeders, 0, 10000, 1},
        {L(en, "后备繁育猫数", "Reserve breeders"), &breeding.reserve_breeders, 0, 10000, 1},
        {L(en, "最低繁育分数", "Minimum breeding score"), &breeding.minimum_score, -10000, 10000, 1},
        {L(en, "繁育最少已知属性", "Breeding known stats"), &breeding.minimum_known_stats, 0, 7, 1},
        {L(en, "繁育资格已确认", "Confirmed breeding eligibility"), &breeding.require_confirmed_eligibility},
        {L(en, "繁育缺失属性惩罚", "Breeding missing penalty"), &breeding.missing_stat_penalty, -10000, 10000, .25},
        {L(en, "繁育力量权重", "Breeding STR weight"), &breeding.stat_weights[0], -10000, 10000, .25},
        {L(en, "繁育敏捷权重", "Breeding DEX weight"), &breeding.stat_weights[1], -10000, 10000, .25},
        {L(en, "繁育体质权重", "Breeding CON weight"), &breeding.stat_weights[2], -10000, 10000, .25},
        {L(en, "繁育智力权重", "Breeding INT weight"), &breeding.stat_weights[3], -10000, 10000, .25},
        {L(en, "繁育速度权重", "Breeding SPD weight"), &breeding.stat_weights[4], -10000, 10000, .25},
        {L(en, "繁育魅力权重", "Breeding CHA weight"), &breeding.stat_weights[5], -10000, 10000, .25},
        {L(en, "繁育幸运权重", "Breeding LCK weight"), &breeding.stat_weights[6], -10000, 10000, .25},
        {L(en, "战斗分类优先", "Combat role first"), &classes.combat_priority_over_breeding},
        {L(en, "最低战斗保留池", "Minimum combat pool"), &classes.minimum_combat_pool, 0, 10000, 1},
        {L(en, "最低繁育保留池", "Minimum breeding pool"), &classes.minimum_breeding_pool, 0, 10000, 1},
        {L(en, "最低普通保留数", "Minimum general reserve"), &classes.minimum_general_reserve, 0, 10000, 1},
        {L(en, "禁止淘汰置信度", "No-cull confidence"), &classes.never_cull_if_data_confidence_below,
         0, 1, .05},
    });
    groups.emplace_back(L(en, "房间、安全与 MOD", "Rooms, Safety & MOD"), std::vector<F>{
        {L(en, "默认软容量", "Default soft capacity"), &rooms.default_soft_capacity, 1, 1000, 1},
        {L(en, "允许软容量溢出", "Allow soft overflow"), &rooms.allow_soft_overflow},
        {L(en, "每房最大软溢出", "Maximum room overflow"), &rooms.max_soft_overflow_per_room, 0, 1000, 1},
        {L(en, "优先单一战斗房", "Prefer one combat room"), &rooms.prefer_single_combat_staging_room},
        {L(en, "繁育配对保持同房", "Keep breeding pair together"), &rooms.keep_breeding_pairs_together},
        {L(en, "避免近亲配对", "Avoid inbreeding pairs"), &rooms.avoid_inbreeding_pairs},
        {L(en, "尽量分开幼猫", "Separate kittens when possible"), &rooms.keep_kittens_separate_when_possible},
        {L(en, "只读模式", "Read-only mode"), &safety.read_only_mode},
        {L(en, "应用前创建备份", "Create backup before apply"), &safety.create_backup_before_apply},
        {L(en, "单击执行模式", "Single-click execution"), &safety.single_click_execute},
        {L(en, "界面语言", "Interface language"), &general.language},
        {L(en, "收集猫数据（默认关闭）", "Collect cat data (off by default)"),
         &diagnostics.collect_cat_data},
        {L(en, "升级重骰次数（重启生效）", "Level-up rerolls (restart)"),
         &level_up.reroll_count, 0, 99, 1},
    });

    std::vector<Page> pages;
    for (auto& [name, fields] : groups) {
        for (std::size_t first = 0; first < fields.size(); first += 8) {
            const auto last = std::min(first + 8, fields.size());
            Page page{name, {}};
            page.fields.insert(
                page.fields.end(), fields.begin() + first, fields.begin() + last);
            pages.push_back(std::move(page));
        }
    }
    return pages;
}

}  // namespace autocattery::ui
