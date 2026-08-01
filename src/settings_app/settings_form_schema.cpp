#include "settings_form_schema.hpp"

#include <array>

namespace autocattery::settings_app {
namespace {

using enum FieldKind;
using enum SettingField;

constexpr std::array kCombatFields{
    FieldSpec{RecommendedCount, L"推荐猫数量", Text},
    FieldSpec{CombatMinimumScore, L"最低战斗分数", Text},
    FieldSpec{CombatMinimumKnownStats, L"最少已知属性数", Text},
    FieldSpec{ExcludeKittens, L"排除幼猫", Check},
    FieldSpec{ExcludeInjured, L"排除受伤猫", Check},
    FieldSpec{CombatConfirmedEligibility, L"要求资格已确认", Check},
    FieldSpec{CombatMissingPenalty, L"缺失属性惩罚", Text},
    FieldSpec{InjuryPenalty, L"受伤惩罚", Text},
    FieldSpec{CombatStrength, L"力量权重", Text},
    FieldSpec{CombatDexterity, L"敏捷权重", Text},
    FieldSpec{CombatConstitution, L"体质权重", Text},
    FieldSpec{CombatIntelligence, L"智力权重", Text},
    FieldSpec{CombatSpeed, L"速度权重", Text},
    FieldSpec{CombatCharisma, L"魅力权重", Text},
    FieldSpec{CombatLuck, L"幸运权重", Text},
    FieldSpec{ShowScore, L"推荐列表显示分数", Check},
    FieldSpec{ShowRank, L"推荐列表显示排名", Check}
};

constexpr std::array kBreedingFields{
    FieldSpec{CoreBreeders, L"核心繁育猫数量", Text},
    FieldSpec{ReserveBreeders, L"后备繁育猫数量", Text},
    FieldSpec{BreedingMinimumScore, L"最低繁育分数", Text},
    FieldSpec{BreedingMinimumKnownStats, L"最少已知属性数", Text},
    FieldSpec{BreedingConfirmedEligibility, L"要求资格已确认", Check},
    FieldSpec{BreedingMissingPenalty, L"缺失属性惩罚", Text},
    FieldSpec{BreedingStrength, L"力量权重", Text},
    FieldSpec{BreedingDexterity, L"敏捷权重", Text},
    FieldSpec{BreedingConstitution, L"体质权重", Text},
    FieldSpec{BreedingIntelligence, L"智力权重", Text},
    FieldSpec{BreedingSpeed, L"速度权重", Text},
    FieldSpec{BreedingCharisma, L"魅力权重", Text},
    FieldSpec{BreedingLuck, L"幸运权重", Text},
    FieldSpec{CombatPriority, L"战斗分类优先于繁育", Check},
    FieldSpec{MinimumCombatPool, L"最低战斗保留池", Text},
    FieldSpec{MinimumBreedingPool, L"最低繁育保留池", Text},
    FieldSpec{MinimumGeneralReserve, L"最低普通保留数", Text},
    FieldSpec{ConfidenceThreshold, L"禁止淘汰置信度阈值", Text}
};

constexpr std::array kPlanningFields{
    FieldSpec{DefaultSoftCapacity, L"默认软容量", Text},
    FieldSpec{AllowSoftOverflow, L"允许软容量溢出", Check},
    FieldSpec{MaximumSoftOverflow, L"每房间最大软溢出", Text},
    FieldSpec{PreferCombatStaging, L"优先单一战斗准备房", Check},
    FieldSpec{KeepBreedingPairs, L"繁育配对保持同房", Check},
    FieldSpec{AvoidInbreedingPairs, L"避免近亲繁育配对", Check},
    FieldSpec{SeparateKittens, L"尽量分开幼猫", Check},
    FieldSpec{ReadOnlyMode, L"只读模式", Check},
    FieldSpec{CreateBackup, L"应用前创建备份", Check},
    FieldSpec{SingleClickExecute, L"单击执行模式（危险）", Check},
    FieldSpec{UseEnglish, L"界面语言：英语（不勾选为中文）", Check},
    FieldSpec{CollectCatData, L"收集猫数据（默认关闭，无姓名/路径/账号）", Check}
};

constexpr std::array kGroups{
    FieldGroup{L"战斗评分与推荐", kCombatFields},
    FieldGroup{L"繁育评分与分类", kBreedingFields},
    FieldGroup{L"房间规则与安全", kPlanningFields}
};

}  // namespace

std::span<const FieldGroup> FieldGroups() {
    return kGroups;
}

}  // namespace autocattery::settings_app
