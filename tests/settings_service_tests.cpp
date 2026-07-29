#include "auto_cattery/settings_service.hpp"

#include <array>

#include "test_support.hpp"

namespace autocattery::tests {

void RunSettingsServiceTests() {
    RuntimeConfigService runtime({}, {});
    AC_CHECK(runtime.LoadInitial().status == ConfigReloadStatus::Applied);
    SettingsService settings(runtime);

    const auto pages = settings.Pages();
    AC_CHECK(pages.size() == 3);
    AC_CHECK(pages[0].kind == SettingsPageKind::Simple);
    AC_CHECK(pages[0].controls.size() == 4);
    AC_CHECK(pages[1].kind == SettingsPageKind::Advanced);
    AC_CHECK(pages[1].controls.size() == snapshot::kStatCount * 2);
    AC_CHECK(pages[2].kind == SettingsPageKind::Safety);
    AC_CHECK(pages[2].controls.size() == 5);
    AC_CHECK(pages[2].controls[3].read_only);
    AC_CHECK(pages[2].controls[4].read_only);

    SimpleSettingsUpdate simple;
    simple.recommended_count = 12;
    simple.exclude_injured = true;
    simple.minimum_general_reserve = 7;
    simple.allow_soft_overflow = false;
    AC_CHECK(static_cast<bool>(settings.ApplySimple(
        simple,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 12);
    AC_CHECK(runtime.Current().recommendation_marker.recommended_count == 12);
    AC_CHECK(runtime.Current().combat_scoring.exclude_injured);
    AC_CHECK(runtime.Current().classification.minimum_general_reserve == 7);
    AC_CHECK(!runtime.Current().room_planning.allow_soft_overflow);

    auto combat_weights = runtime.Current().combat_scoring.stat_weights;
    combat_weights[0] = 2.5;
    auto breeding_weights = runtime.Current().breeding_scoring.stat_weights;
    breeding_weights[6] = -1.25;
    AdvancedSettingsUpdate advanced;
    advanced.combat_stat_weights = combat_weights;
    advanced.breeding_stat_weights = breeding_weights;
    AC_CHECK(static_cast<bool>(settings.ApplyAdvanced(
        advanced,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.stat_weights[0] == 2.5);
    AC_CHECK(runtime.Current().breeding_scoring.stat_weights[6] == -1.25);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 12);

    SafetySettingsUpdate unconfirmed;
    unconfirmed.single_click_execute = true;
    AC_CHECK(!static_cast<bool>(settings.ApplySafety(
        unconfirmed,
        workflow::WorkflowState::Idle)));
    AC_CHECK(!runtime.Current().execution_safety.single_click_execute);

    SafetySettingsUpdate confirmed;
    confirmed.single_click_execute = true;
    confirmed.confirm_single_click_enable = true;
    confirmed.read_only_mode = false;
    confirmed.create_backup_before_apply = false;
    AC_CHECK(static_cast<bool>(settings.ApplySafety(
        confirmed,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().execution_safety.single_click_execute);
    AC_CHECK(!runtime.Current().execution_safety.read_only_mode);
    AC_CHECK(!runtime.Current().execution_safety.create_backup_before_apply);
    AC_CHECK(!runtime.Current().execution.cull_enabled);
    AC_CHECK(runtime.Current().recommendation_marker.never_auto_select);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 12);

    SimpleSettingsUpdate invalid;
    invalid.recommended_count = 101;
    AC_CHECK(!static_cast<bool>(settings.ApplySimple(
        invalid,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 12);

    SimpleSettingsUpdate deferred;
    deferred.recommended_count = 9;
    const auto busy = settings.ApplySimple(
        deferred,
        workflow::WorkflowState::Scoring);
    AC_CHECK(!static_cast<bool>(busy));
    AC_CHECK(busy.code == ErrorCode::WriteConflict);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 12);
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 9);

    AC_CHECK(static_cast<bool>(settings.AdjustControl(
        "combat_scoring.recommended_count",
        -1,
        false,
        false,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 8);
    AC_CHECK(static_cast<bool>(settings.AdjustControl(
        "combat_scoring.stat_weights.strength",
        -1,
        false,
        false,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.stat_weights[0] == 2.25);

    const auto unknown_stat = settings.AdjustControl(
        "combat_scoring.stat_weights.not_strength",
        1,
        false,
        false,
        workflow::WorkflowState::Idle);
    AC_CHECK(!static_cast<bool>(unknown_stat));
    AC_CHECK(unknown_stat.code == ErrorCode::ConfigInvalid);
    AC_CHECK(runtime.Current().combat_scoring.stat_weights[0] == 2.25);

    const auto locked = settings.AdjustControl(
        "recommendation_marker.never_auto_select",
        1,
        true,
        false,
        workflow::WorkflowState::Idle);
    AC_CHECK(!static_cast<bool>(locked));
    AC_CHECK(locked.code == ErrorCode::ConfigInvalid);
    AC_CHECK(runtime.Current().recommendation_marker.never_auto_select);
}

}  // namespace autocattery::tests
