#pragma once

#include <span>

namespace autocattery::settings_app {

enum class SettingField : int {
    RecommendedCount = 2001,
    CombatMinimumScore,
    CombatMinimumKnownStats,
    ExcludeKittens,
    ExcludeInjured,
    CombatConfirmedEligibility,
    CombatMissingPenalty,
    InjuryPenalty,
    CombatStrength,
    CombatDexterity,
    CombatConstitution,
    CombatIntelligence,
    CombatSpeed,
    CombatCharisma,
    CombatLuck,
    ShowScore,
    ShowRank,

    CoreBreeders = 2101,
    ReserveBreeders,
    BreedingMinimumScore,
    BreedingMinimumKnownStats,
    BreedingConfirmedEligibility,
    BreedingMissingPenalty,
    BreedingStrength,
    BreedingDexterity,
    BreedingConstitution,
    BreedingIntelligence,
    BreedingSpeed,
    BreedingCharisma,
    BreedingLuck,
    CombatPriority,
    MinimumCombatPool,
    MinimumBreedingPool,
    MinimumGeneralReserve,
    ConfidenceThreshold,

    DefaultSoftCapacity = 2201,
    AllowSoftOverflow,
    MaximumSoftOverflow,
    PreferCombatStaging,
    KeepBreedingPairs,
    AvoidInbreedingPairs,
    SeparateKittens,
    ReadOnlyMode,
    CreateBackup,
    SingleClickExecute
};

enum class FieldKind { Text, Check };

struct FieldSpec {
    SettingField field;
    const wchar_t* label;
    FieldKind kind;
};

struct FieldGroup {
    const wchar_t* title;
    std::span<const FieldSpec> fields;
};

[[nodiscard]] std::span<const FieldGroup> FieldGroups();

}  // namespace autocattery::settings_app
