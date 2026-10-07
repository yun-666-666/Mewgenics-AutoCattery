#include "auto_cattery/config_runtime.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

struct RuntimeFixture {
    std::filesystem::path directory{
        std::filesystem::temp_directory_path() /
        "auto_cattery_stage13_runtime_tests"};
    std::filesystem::path user{directory / "user.json"};
    std::filesystem::file_time_type write_time{
        std::filesystem::file_time_type::clock::now()};
    std::chrono::steady_clock::time_point now{
        std::chrono::steady_clock::time_point{} + std::chrono::seconds(1)};

    RuntimeFixture() {
        std::filesystem::create_directories(directory);
        Write("{}");
    }

    void Write(const std::string& text) {
        std::ofstream stream(user, std::ios::binary | std::ios::trunc);
        stream << text;
        stream.close();
        write_time += std::chrono::seconds(1);
        std::filesystem::last_write_time(user, write_time);
    }
};

}  // namespace

void RunConfigRuntimeTests() {
    using namespace std::chrono_literals;
    RuntimeFixture fixture;
    std::vector<ConfigInvalidation> invalidations;
    RuntimeConfigService runtime(
        {},
        fixture.user,
        [&fixture] { return fixture.now; },
        [&invalidations](const auto& invalidation) {
            invalidations.push_back(invalidation);
        });

    const auto initial = runtime.LoadInitial();
    AC_CHECK(initial.status == ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 8);
    const auto initial_generation = runtime.Generation();

    fixture.Write(R"({"combat_scoring":{"recommended_count":3}})");
    const auto waiting = runtime.PollHotReload(workflow::WorkflowState::Idle);
    AC_CHECK(waiting.status == ConfigReloadStatus::WaitingForDebounce);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 8);
    fixture.now += 499ms;
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::WaitingForDebounce);
    fixture.now += 1ms;
    const auto applied = runtime.PollHotReload(workflow::WorkflowState::Idle);
    AC_CHECK(applied.status == ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 3);
    AC_CHECK(runtime.Generation() == initial_generation + 1);
    AC_CHECK(invalidations.size() == 1);
    AC_CHECK(invalidations.back().previous_digest !=
             invalidations.back().new_digest);

    fixture.Write(R"({"combat_scoring":{"recommended_count":4}})");
    const auto busy = runtime.PollHotReload(workflow::WorkflowState::Scoring);
    AC_CHECK(busy.status == ConfigReloadStatus::DeferredBusy);
    fixture.now += 1s;
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Planning).status ==
             ConfigReloadStatus::DeferredBusy);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 3);
    const auto after_busy =
        runtime.PollHotReload(workflow::WorkflowState::Idle);
    AC_CHECK(after_busy.status == ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 4);

    const auto stable_generation = runtime.Generation();
    fixture.Write("{");
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::WaitingForDebounce);
    fixture.now += 500ms;
    const auto rejected =
        runtime.PollHotReload(workflow::WorkflowState::Idle);
    AC_CHECK(rejected.status == ConfigReloadStatus::Rejected);
    AC_CHECK(runtime.Generation() == stable_generation);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 4);
    AC_CHECK(!runtime.LastError().empty());
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::Unchanged);

    fixture.Write(R"({"combat_scoring":{"recommended_count":5}})");
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::WaitingForDebounce);
    fixture.now += 500ms;
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::Applied);
    AC_CHECK(runtime.LastError().empty());

    SessionConfigOverride simple;
    simple.combat_exclude_injured = true;
    AC_CHECK(runtime.ApplySessionOverride(
        simple,
        workflow::WorkflowState::Idle).status == ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.exclude_injured);

    SessionConfigOverride safety;
    safety.read_only_mode = false;
    AC_CHECK(runtime.ApplySessionOverride(
        safety,
        workflow::WorkflowState::Idle).status == ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.exclude_injured);
    AC_CHECK(!runtime.Current().execution_safety.read_only_mode);

    SessionConfigOverride invalid;
    invalid.combat_recommended_count = 0;
    const auto invalid_result = runtime.ApplySessionOverride(
        invalid,
        workflow::WorkflowState::Idle);
    AC_CHECK(invalid_result.status == ConfigReloadStatus::Rejected);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 5);

    SessionConfigOverride deferred;
    deferred.combat_recommended_count = 6;
    AC_CHECK(runtime.ApplySessionOverride(
        deferred,
        workflow::WorkflowState::Applying).status ==
        ConfigReloadStatus::DeferredBusy);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 5);
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 6);
    AC_CHECK(runtime.Current().combat_scoring.exclude_injured);

    // The live organizer remains Completed after a successful organize.
    // Settings must also invalidate an unconfirmed preview, not wait forever.
    for (const auto state : {workflow::WorkflowState::Completed,
             workflow::WorkflowState::AwaitingConfirmation,
             workflow::WorkflowState::Failed, workflow::WorkflowState::Cancelled}) {
        fixture.Write(R"({"room_planning":{"breeding_room_population":7,"prefer_single_combat_staging_room":true}})");
        AC_CHECK(runtime.RequestReload(state).status == ConfigReloadStatus::Applied);
        fixture.Write(R"({"room_planning":{"breeding_room_population":6,"prefer_single_combat_staging_room":false}})");
        AC_CHECK(runtime.RequestReload(state).status == ConfigReloadStatus::Applied);
        AC_CHECK(runtime.Current().room_planning.breeding_room_population == 6);
        AC_CHECK(!runtime.Current().room_planning.prefer_single_combat_staging_room);
    }
    // Changing only the newborn intervention must invalidate the live config.
    for (const bool enabled : {true, false}) {
        fixture.Write(enabled
            ? R"({"room_planning":{"offspring_all_seven_assist":true}})"
            : R"({"room_planning":{"offspring_all_seven_assist":false}})");
        const auto generation = runtime.Generation();
        AC_CHECK(runtime.RequestReload(workflow::WorkflowState::Idle).status ==
                 ConfigReloadStatus::Applied);
        AC_CHECK(runtime.Current().room_planning.offspring_all_seven_assist == enabled);
        AC_CHECK(runtime.Generation() == generation + 1);
    }
    for (const bool enabled : {true, false}) {
        fixture.Write(enabled
            ? R"({"room_planning":{"food_supply_assist":true}})"
            : R"({"room_planning":{"food_supply_assist":false}})");
        const auto generation = runtime.Generation();
        AC_CHECK(runtime.RequestReload(workflow::WorkflowState::Idle).status ==
                 ConfigReloadStatus::Applied);
        AC_CHECK(runtime.Current().room_planning.food_supply_assist == enabled);
        AC_CHECK(!runtime.Current().room_planning.offspring_all_seven_assist);
        AC_CHECK(runtime.Generation() == generation + 1);
    }
}

}  // namespace autocattery::tests
