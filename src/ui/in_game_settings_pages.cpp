#include "in_game_settings_model.hpp"

#include <algorithm>

namespace autocattery::ui {

std::vector<InGameSettingsModel::Page> InGameSettingsModel::Pages() {
    using F = Field;
    auto& combat = config_.combat_scoring;
    auto& breeding = config_.breeding_scoring;
    auto& classes = config_.classification;
    auto& rooms = config_.room_planning;
    auto& marker = config_.recommendation_marker;
    auto& safety = config_.execution_safety;
    std::vector<std::pair<const char*, std::vector<F>>> groups;
    groups.emplace_back("战斗评分", std::vector<F>{
        {"推荐猫数量", &combat.recommended_count, 1, 100, 1},
        {"最低战斗分数", &combat.minimum_score, -10000, 10000, 1},
        {"最少已知属性", &combat.minimum_known_stats, 0, 7, 1},
        {"排除幼猫", &combat.exclude_kittens},
        {"排除受伤猫", &combat.exclude_injured},
        {"要求资格已确认", &combat.require_confirmed_eligibility},
        {"缺失属性惩罚", &combat.missing_stat_penalty, -10000, 10000, .25},
        {"受伤惩罚", &combat.injury_penalty, -10000, 10000, .25},
        {"力量权重", &combat.stat_weights[0], -10000, 10000, .25},
        {"敏捷权重", &combat.stat_weights[1], -10000, 10000, .25},
        {"体质权重", &combat.stat_weights[2], -10000, 10000, .25},
        {"智力权重", &combat.stat_weights[3], -10000, 10000, .25},
        {"速度权重", &combat.stat_weights[4], -10000, 10000, .25},
        {"魅力权重", &combat.stat_weights[5], -10000, 10000, .25},
        {"幸运权重", &combat.stat_weights[6], -10000, 10000, .25},
        {"推荐显示分数", &marker.show_score},
        {"推荐显示排名", &marker.show_rank},
    });
    groups.emplace_back("繁育与分类", std::vector<F>{
        {"核心繁育猫数", &breeding.core_breeders, 0, 10000, 1},
        {"后备繁育猫数", &breeding.reserve_breeders, 0, 10000, 1},
        {"最低繁育分数", &breeding.minimum_score, -10000, 10000, 1},
        {"繁育最少已知属性", &breeding.minimum_known_stats, 0, 7, 1},
        {"繁育资格已确认", &breeding.require_confirmed_eligibility},
        {"繁育缺失属性惩罚", &breeding.missing_stat_penalty, -10000, 10000, .25},
        {"繁育力量权重", &breeding.stat_weights[0], -10000, 10000, .25},
        {"繁育敏捷权重", &breeding.stat_weights[1], -10000, 10000, .25},
        {"繁育体质权重", &breeding.stat_weights[2], -10000, 10000, .25},
        {"繁育智力权重", &breeding.stat_weights[3], -10000, 10000, .25},
        {"繁育速度权重", &breeding.stat_weights[4], -10000, 10000, .25},
        {"繁育魅力权重", &breeding.stat_weights[5], -10000, 10000, .25},
        {"繁育幸运权重", &breeding.stat_weights[6], -10000, 10000, .25},
        {"战斗分类优先", &classes.combat_priority_over_breeding},
        {"最低战斗保留池", &classes.minimum_combat_pool, 0, 10000, 1},
        {"最低繁育保留池", &classes.minimum_breeding_pool, 0, 10000, 1},
        {"最低普通保留数", &classes.minimum_general_reserve, 0, 10000, 1},
        {"禁止淘汰置信度", &classes.never_cull_if_data_confidence_below,
         0, 1, .05},
    });
    groups.emplace_back("房间与执行", std::vector<F>{
        {"默认软容量", &rooms.default_soft_capacity, 1, 1000, 1},
        {"允许软容量溢出", &rooms.allow_soft_overflow},
        {"每房最大软溢出", &rooms.max_soft_overflow_per_room, 0, 1000, 1},
        {"优先单一战斗房", &rooms.prefer_single_combat_staging_room},
        {"繁育配对保持同房", &rooms.keep_breeding_pairs_together},
        {"避免近亲配对", &rooms.avoid_inbreeding_pairs},
        {"尽量分开幼猫", &rooms.keep_kittens_separate_when_possible},
        {"只读模式", &safety.read_only_mode},
        {"应用前创建备份", &safety.create_backup_before_apply},
        {"单击执行模式", &safety.single_click_execute},
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
