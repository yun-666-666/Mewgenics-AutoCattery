#include "settings_form.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

#include "settings_form_schema.hpp"

namespace autocattery::settings_app {
namespace {

int ControlId(SettingField field) {
    return static_cast<int>(field);
}

void SetText(HWND parent, SettingField field, std::wstring_view value) {
    SetDlgItemTextW(parent, ControlId(field), std::wstring(value).c_str());
}

void SetSize(HWND parent, SettingField field, std::size_t value) {
    SetText(parent, field, std::to_wstring(value));
}

void SetDouble(HWND parent, SettingField field, double value) {
    std::wostringstream text;
    text << std::setprecision(12) << value;
    SetText(parent, field, text.str());
}

void SetCheck(HWND parent, SettingField field, bool value) {
    CheckDlgButton(
        parent,
        ControlId(field),
        value ? BST_CHECKED : BST_UNCHECKED);
}

bool IsChecked(HWND parent, SettingField field) {
    return IsDlgButtonChecked(parent, ControlId(field)) == BST_CHECKED;
}

std::wstring GetText(HWND parent, SettingField field) {
    const auto length = GetWindowTextLengthW(
        GetDlgItem(parent, ControlId(field)));
    std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
    const auto copied = GetDlgItemTextW(
        parent,
        ControlId(field),
        text.data(),
        static_cast<int>(text.size()));
    text.resize(static_cast<std::size_t>(copied));
    return text;
}

Result<void> ReadSize(
    HWND parent,
    SettingField field,
    std::size_t& destination,
    const char* name) {
    const auto text = GetText(parent, field);
    if (text.empty() || text.front() == L'-') {
        return {ErrorCode::ConfigInvalid, std::string(name) + " must be an integer"};
    }
    wchar_t* end{};
    errno = 0;
    const auto value = std::wcstoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != L'\0' ||
        value > std::numeric_limits<std::size_t>::max()) {
        return {ErrorCode::ConfigInvalid, std::string(name) + " must be an integer"};
    }
    destination = static_cast<std::size_t>(value);
    return {};
}

Result<void> ReadDouble(
    HWND parent,
    SettingField field,
    double& destination,
    const char* name) {
    const auto text = GetText(parent, field);
    wchar_t* end{};
    errno = 0;
    const auto value = std::wcstod(text.c_str(), &end);
    if (text.empty() || errno == ERANGE || end == text.c_str() ||
        *end != L'\0' || !std::isfinite(value)) {
        return {ErrorCode::ConfigInvalid,
                std::string(name) + " must be a finite number"};
    }
    destination = value;
    return {};
}

template<class Reader>
Result<void> ReadAll(std::initializer_list<Reader> readers) {
    for (const auto& reader : readers) {
        const auto result = reader();
        if (!result) {
            return result;
        }
    }
    return {};
}

}  // namespace

void PopulateSettingsForm(HWND parent, const Config& config) {
    using enum SettingField;
    SetSize(parent, RecommendedCount, config.combat_scoring.recommended_count);
    SetDouble(parent, CombatMinimumScore, config.combat_scoring.minimum_score);
    SetSize(parent, CombatMinimumKnownStats, config.combat_scoring.minimum_known_stats);
    SetCheck(parent, ExcludeKittens, config.combat_scoring.exclude_kittens);
    SetCheck(parent, ExcludeInjured, config.combat_scoring.exclude_injured);
    SetCheck(parent, CombatConfirmedEligibility,
             config.combat_scoring.require_confirmed_eligibility);
    SetDouble(parent, CombatMissingPenalty, config.combat_scoring.missing_stat_penalty);
    SetDouble(parent, InjuryPenalty, config.combat_scoring.injury_penalty);
    const std::array combat_fields{
        CombatStrength, CombatDexterity, CombatConstitution, CombatIntelligence,
        CombatSpeed, CombatCharisma, CombatLuck
    };
    for (std::size_t index = 0; index < combat_fields.size(); ++index) {
        SetDouble(parent, combat_fields[index], config.combat_scoring.stat_weights[index]);
    }
    SetCheck(parent, ShowScore, config.recommendation_marker.show_score);
    SetCheck(parent, ShowRank, config.recommendation_marker.show_rank);

    SetSize(parent, CoreBreeders, config.breeding_scoring.core_breeders);
    SetSize(parent, ReserveBreeders, config.breeding_scoring.reserve_breeders);
    SetDouble(parent, BreedingMinimumScore, config.breeding_scoring.minimum_score);
    SetSize(parent, BreedingMinimumKnownStats,
            config.breeding_scoring.minimum_known_stats);
    SetCheck(parent, BreedingConfirmedEligibility,
             config.breeding_scoring.require_confirmed_eligibility);
    SetDouble(parent, BreedingMissingPenalty,
              config.breeding_scoring.missing_stat_penalty);
    const std::array breeding_fields{
        BreedingStrength, BreedingDexterity, BreedingConstitution,
        BreedingIntelligence, BreedingSpeed, BreedingCharisma, BreedingLuck
    };
    for (std::size_t index = 0; index < breeding_fields.size(); ++index) {
        SetDouble(parent, breeding_fields[index],
                  config.breeding_scoring.stat_weights[index]);
    }
    SetCheck(parent, CombatPriority,
             config.classification.combat_priority_over_breeding);
    SetSize(parent, MinimumCombatPool, config.classification.minimum_combat_pool);
    SetSize(parent, MinimumBreedingPool, config.classification.minimum_breeding_pool);
    SetSize(parent, MinimumGeneralReserve,
            config.classification.minimum_general_reserve);
    SetDouble(parent, ConfidenceThreshold,
              config.classification.never_cull_if_data_confidence_below);

    SetSize(parent, DefaultSoftCapacity, config.room_planning.default_soft_capacity);
    SetCheck(parent, AllowSoftOverflow, config.room_planning.allow_soft_overflow);
    SetSize(parent, MaximumSoftOverflow,
            config.room_planning.max_soft_overflow_per_room);
    SetCheck(parent, PreferCombatStaging,
             config.room_planning.prefer_single_combat_staging_room);
    SetCheck(parent, KeepBreedingPairs,
             config.room_planning.keep_breeding_pairs_together);
    SetCheck(parent, AvoidInbreedingPairs, config.room_planning.avoid_inbreeding_pairs);
    SetCheck(parent, SeparateKittens,
             config.room_planning.keep_kittens_separate_when_possible);
    SetCheck(parent, ReadOnlyMode, config.execution_safety.read_only_mode);
    SetCheck(parent, CreateBackup, config.execution_safety.create_backup_before_apply);
    SetCheck(parent, SingleClickExecute,
             config.execution_safety.single_click_execute);
    SetCheck(parent, UseEnglish, config.general.language == "en-US");
    SetCheck(parent, CollectCatData, config.diagnostics.collect_cat_data);
}

Result<Config> ReadSettingsForm(HWND parent, const Config& base) {
    using enum SettingField;
    auto config = base;
    const auto read = ReadAll<std::function<Result<void>()>>({
        [&] { return ReadSize(parent, RecommendedCount,
                             config.combat_scoring.recommended_count,
                             "recommended_count"); },
        [&] { return ReadDouble(parent, CombatMinimumScore,
                               config.combat_scoring.minimum_score,
                               "combat minimum_score"); },
        [&] { return ReadSize(parent, CombatMinimumKnownStats,
                             config.combat_scoring.minimum_known_stats,
                             "combat minimum_known_stats"); },
        [&] { return ReadDouble(parent, CombatMissingPenalty,
                               config.combat_scoring.missing_stat_penalty,
                               "combat missing_stat_penalty"); },
        [&] { return ReadDouble(parent, InjuryPenalty,
                               config.combat_scoring.injury_penalty,
                               "injury_penalty"); },
        [&] { return ReadSize(parent, CoreBreeders,
                             config.breeding_scoring.core_breeders,
                             "core_breeders"); },
        [&] { return ReadSize(parent, ReserveBreeders,
                             config.breeding_scoring.reserve_breeders,
                             "reserve_breeders"); },
        [&] { return ReadDouble(parent, BreedingMinimumScore,
                               config.breeding_scoring.minimum_score,
                               "breeding minimum_score"); },
        [&] { return ReadSize(parent, BreedingMinimumKnownStats,
                             config.breeding_scoring.minimum_known_stats,
                             "breeding minimum_known_stats"); },
        [&] { return ReadDouble(parent, BreedingMissingPenalty,
                               config.breeding_scoring.missing_stat_penalty,
                               "breeding missing_stat_penalty"); },
        [&] { return ReadSize(parent, MinimumCombatPool,
                             config.classification.minimum_combat_pool,
                             "minimum_combat_pool"); },
        [&] { return ReadSize(parent, MinimumBreedingPool,
                             config.classification.minimum_breeding_pool,
                             "minimum_breeding_pool"); },
        [&] { return ReadSize(parent, MinimumGeneralReserve,
                             config.classification.minimum_general_reserve,
                             "minimum_general_reserve"); },
        [&] { return ReadDouble(parent, ConfidenceThreshold,
                               config.classification.never_cull_if_data_confidence_below,
                               "confidence threshold"); },
        [&] { return ReadSize(parent, DefaultSoftCapacity,
                             config.room_planning.default_soft_capacity,
                             "default_soft_capacity"); },
        [&] { return ReadSize(parent, MaximumSoftOverflow,
                             config.room_planning.max_soft_overflow_per_room,
                             "max_soft_overflow_per_room"); }
    });
    if (!read) {
        return {{}, read.code, read.message};
    }

    const std::array combat_fields{
        CombatStrength, CombatDexterity, CombatConstitution, CombatIntelligence,
        CombatSpeed, CombatCharisma, CombatLuck
    };
    const std::array breeding_fields{
        BreedingStrength, BreedingDexterity, BreedingConstitution,
        BreedingIntelligence, BreedingSpeed, BreedingCharisma, BreedingLuck
    };
    for (std::size_t index = 0; index < combat_fields.size(); ++index) {
        auto result = ReadDouble(parent, combat_fields[index],
                                 config.combat_scoring.stat_weights[index],
                                 "combat stat weight");
        if (!result) {
            return {{}, result.code, result.message};
        }
        result = ReadDouble(parent, breeding_fields[index],
                            config.breeding_scoring.stat_weights[index],
                            "breeding stat weight");
        if (!result) {
            return {{}, result.code, result.message};
        }
    }

    config.combat_scoring.exclude_kittens = IsChecked(parent, ExcludeKittens);
    config.combat_scoring.exclude_injured = IsChecked(parent, ExcludeInjured);
    config.combat_scoring.require_confirmed_eligibility =
        IsChecked(parent, CombatConfirmedEligibility);
    config.recommendation_marker.recommended_count =
        config.combat_scoring.recommended_count;
    config.recommendation_marker.show_score = IsChecked(parent, ShowScore);
    config.recommendation_marker.show_rank = IsChecked(parent, ShowRank);
    config.breeding_scoring.require_confirmed_eligibility =
        IsChecked(parent, BreedingConfirmedEligibility);
    config.classification.combat_priority_over_breeding =
        IsChecked(parent, CombatPriority);
    config.room_planning.allow_soft_overflow = IsChecked(parent, AllowSoftOverflow);
    config.room_planning.prefer_single_combat_staging_room =
        IsChecked(parent, PreferCombatStaging);
    config.room_planning.keep_breeding_pairs_together =
        IsChecked(parent, KeepBreedingPairs);
    config.room_planning.avoid_inbreeding_pairs =
        IsChecked(parent, AvoidInbreedingPairs);
    config.room_planning.keep_kittens_separate_when_possible =
        IsChecked(parent, SeparateKittens);
    config.execution_safety.read_only_mode = IsChecked(parent, ReadOnlyMode);
    config.execution_safety.create_backup_before_apply = IsChecked(parent, CreateBackup);
    config.execution_safety.single_click_execute = IsChecked(parent, SingleClickExecute);
    config.general.language = IsChecked(parent, UseEnglish)
        ? "en-US" : "zh-CN";
    config.language = config.general.language;
    config.diagnostics.collect_cat_data = IsChecked(parent, CollectCatData);
    config.safety = config.execution_safety;
    return {std::move(config)};
}

}  // namespace autocattery::settings_app
