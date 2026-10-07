#include "auto_cattery/config.hpp"
#include "auto_cattery/breeding/offspring_assistance.hpp"

#include <filesystem>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::filesystem::path TestDirectory() {
    const auto directory =
        std::filesystem::temp_directory_path() / "auto_cattery_phase01_tests";
    std::filesystem::create_directories(directory);
    return directory;
}

void Write(const std::filesystem::path& path, const char* contents) {
    std::ofstream stream(path, std::ios::trunc);
    stream << contents;
}

}  // namespace

void RunConfigTests() {
    std::array<std::int32_t, 7> genetic{1, 2, 3, 4, 5, 6, 7};
    const auto original = genetic;
    AC_CHECK(!breeding::ApplyOffspringAssistance(genetic, false));
    AC_CHECK(genetic == original);
    AC_CHECK(breeding::ApplyOffspringAssistance(genetic, true));
    AC_CHECK((genetic == std::array<std::int32_t, 7>{7, 7, 7, 7, 7, 7, 7}));
    AC_CHECK(!breeding::ApplyOffspringAssistance(genetic, true));
    const auto directory = TestDirectory();
    const auto defaults = directory / "default.json";
    const auto user = directory / "user.json";

    Write(defaults, R"({
        "schema_version": 1,
        "mod_enabled": true,
        "safe_mode": true,
        "log_level": "info",
        "language": "zh-CN",
        "ui": {},
        "safety": {
            "require_preview_before_destructive_actions": true,
            "create_backup_before_apply": true,
            "abort_on_unknown_game_build": true
        }
    })");
    Write(user, R"({"language":"en-US","unknown_future_field":42})");

    const auto valid = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(valid));
    AC_CHECK(valid.value.language == "en-US");
    AC_CHECK(valid.value.safe_mode);
    AC_CHECK(valid.value.combat_scoring.recommended_count == 8);
    AC_CHECK(valid.value.combat_scoring.require_confirmed_eligibility);
    AC_CHECK(valid.value.combat_scoring.stat_weights.size() == 7);
    AC_CHECK(valid.value.combat_scoring.stat_weights[5] == 1.0);
    AC_CHECK(valid.value.breeding_scoring.core_breeders == 4);
    AC_CHECK(valid.value.breeding_scoring.require_confirmed_eligibility);
    AC_CHECK(valid.value.breeding_scoring.stat_weights.size() == 7);
    AC_CHECK(valid.value.classification.minimum_combat_pool == 8);
    AC_CHECK(
        valid.value.classification.never_cull_if_data_confidence_below ==
        0.85);
    AC_CHECK(valid.value.protection.version == 1);
    AC_CHECK(valid.value.protection.protect_unknown_native_state);
    AC_CHECK(valid.value.protection.require_stable_identity_for_sidecar);
    AC_CHECK(valid.value.room_planning.version == 1);
    AC_CHECK(valid.value.room_planning.default_soft_capacity == 4);
    AC_CHECK(valid.value.room_planning.breeding_room_population == 4);
    AC_CHECK(valid.value.room_planning.never_exceed_known_hard_capacity);
    AC_CHECK(valid.value.workflow.preview_ttl_seconds == 120);
    AC_CHECK(valid.value.level_up.reroll_count == 3);

    Write(user, R"({
        "combat_scoring": {
            "recommended_count": 3,
            "minimum_score": 50,
            "stat_weights": {"charisma": -0.5},
            "active_ability_overrides": {"Fireball": 4.25}
        }
    })");
    const auto scoring = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(scoring));
    AC_CHECK(scoring.value.combat_scoring.recommended_count == 3);
    AC_CHECK(scoring.value.combat_scoring.minimum_score == 50.0);
    AC_CHECK(scoring.value.combat_scoring.stat_weights[5] == -0.5);
    AC_CHECK(
        scoring.value.combat_scoring.active_ability_overrides.at("Fireball") ==
        4.25);

    Write(user, R"({
        "breeding_scoring": {
            "core_breeders": 3,
            "stat_weights": {"charisma": 2.0},
            "disorder_overrides": {"Fragile": 5.0}
        },
        "classification": {
            "minimum_general_reserve": 6,
            "never_cull_if_data_confidence_below": 0.95
        }
    })");
    const auto breeding = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(breeding));
    AC_CHECK(breeding.value.breeding_scoring.core_breeders == 3);
    AC_CHECK(breeding.value.breeding_scoring.stat_weights[5] == 2.0);
    AC_CHECK(
        breeding.value.breeding_scoring.disorder_overrides.at("Fragile") ==
        5.0);
    AC_CHECK(breeding.value.classification.minimum_general_reserve == 6);

    Write(user, R"({"schema_version":99})");
    const auto future = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(future));
    AC_CHECK(future.value.force_read_only);

    Write(user, R"({"safety":{"create_backup_before_apply":false}})");
    const auto backup_disabled = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(backup_disabled));
    AC_CHECK(!backup_disabled.value.execution_safety.create_backup_before_apply);
    AC_CHECK(!backup_disabled.value.execution.cull_enabled);

    Write(user, R"({"execution":{"real_write_adapter_enabled":true}})");
    const auto unverified_execution = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unverified_execution));
    AC_CHECK(unverified_execution.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"combat_scoring":{"minimum_known_stats":8}})");
    const auto impossible = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(impossible));
    AC_CHECK(impossible.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"combat_scoring":{"minimum_score":1e999}})");
    const auto non_finite = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(non_finite));
    AC_CHECK(non_finite.code == ErrorCode::ConfigInvalid);

    Write(
        user,
        R"({"classification":{"never_cull_if_data_confidence_below":1.1}})");
    const auto unsafe_threshold = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unsafe_threshold));
    AC_CHECK(unsafe_threshold.code == ErrorCode::ConfigInvalid);

    Write(
        user,
        R"({"protection":{"protect_unknown_native_state":false}})");
    const auto unsafe_protection = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unsafe_protection));
    AC_CHECK(unsafe_protection.code == ErrorCode::ConfigInvalid);

    Write(
        user,
        R"({"room_planning":{"never_exceed_known_hard_capacity":false}})");
    const auto unsafe_capacity = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unsafe_capacity));
    AC_CHECK(unsafe_capacity.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"room_planning":{"default_soft_capacity":0}})");
    const auto invalid_soft_preference = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(invalid_soft_preference));
    AC_CHECK(invalid_soft_preference.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"room_planning":{"allow_partial_plan":false}})");
    const auto unsafe_partial = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unsafe_partial));
    AC_CHECK(unsafe_partial.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"workflow":{"preview_ttl_seconds":9}})");
    const auto invalid_ttl = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(invalid_ttl));
    AC_CHECK(invalid_ttl.code == ErrorCode::ConfigInvalid);

    Write(user, R"({"level_up":{"reroll_count":0}})");
    const auto zero_rerolls = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(zero_rerolls));
    AC_CHECK(zero_rerolls.value.level_up.reroll_count == 0);

    Write(user, R"({"level_up":{"reroll_count":99}})");
    const auto maximum_rerolls = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(maximum_rerolls));
    AC_CHECK(maximum_rerolls.value.level_up.reroll_count == 99);

    Write(user, R"({"level_up":{"reroll_count":100}})");
    const auto excessive_rerolls = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(excessive_rerolls));
    AC_CHECK(excessive_rerolls.code == ErrorCode::ConfigInvalid);

    Write(user, "{");
    const auto truncated = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(truncated));
}

}  // namespace autocattery::tests
