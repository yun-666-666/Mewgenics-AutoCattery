#include "auto_cattery/workflow/organize_workflow_facade.hpp"

#include "test_support.hpp"

#include <memory>

namespace autocattery::tests {
namespace {

class FakeReadAdapter final
    : public snapshot::IGameReadAdapter {
public:
    Result<snapshot::HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) override {
        captured_generation = scene_generation;
        if (fail) {
            return {
                {},
                ErrorCode::CatDataUnavailable,
                "fixture read failed"
            };
        }
        snapshot::HouseSnapshot house;
        house.snapshot_id = 1;
        house.scene_generation = scene_generation;
        house.cats = {
            {.id = 11, .display_name = "private-name",
             .room_id = "Floor1_Large"},
            {.id = 12, .display_name = "other-private-name"}
        };
        house.rooms = {
            {.id = "Floor1_Large", .residents = {11}}
        };
        return {std::move(house)};
    }

    bool fail{};
    std::uint64_t captured_generation{};
};

}  // namespace

void RunOrganizeWorkflowFacadeTests() {
    auto configurable_adapter = std::make_unique<FakeReadAdapter>();
    workflow::OrganizeWorkflowFacade configurable(
        std::move(configurable_adapter));
    Config updated_config;
    updated_config.combat_scoring.recommended_count = 3;
    AC_CHECK(static_cast<bool>(configurable.ApplyConfig(updated_config)));

    auto adapter = std::make_unique<FakeReadAdapter>();
    auto* adapter_view = adapter.get();
    workflow::OrganizeWorkflowFacade workflow(std::move(adapter));
    const auto preview = workflow.RequestPreview(44);
    AC_CHECK(static_cast<bool>(preview));
    AC_CHECK(adapter_view->captured_generation == 44);
    AC_CHECK(
        preview.message.find("cats=2") != std::string::npos);
    AC_CHECK(
        preview.message.find("private-name") == std::string::npos);
    AC_CHECK(
        workflow.CurrentExecutionAvailability() ==
        autocattery::workflow::WorkflowCapability::PreviewOnly);
    AC_CHECK(
        workflow.State() ==
        autocattery::workflow::WorkflowState::AwaitingConfirmation);
    AC_CHECK(static_cast<bool>(workflow.ApplyConfig(Config{})));
    AC_CHECK(!workflow.LatestPreview());
    const auto execution = workflow.RequestExecution();
    AC_CHECK(!static_cast<bool>(execution));
    AC_CHECK(execution.code == ErrorCode::UnsupportedGameBuild);

    const auto detailed = workflow.BuildPreview(44);
    AC_CHECK(static_cast<bool>(detailed));
    AC_CHECK(!detailed.value.id.empty());
    AC_CHECK(detailed.value.game_data_modified == false);
    const auto unavailable = workflow.Execute(
        detailed.value.id,
        workflow::ExecutionChoice::Execute,
        detailed.value.bindings);
    AC_CHECK(static_cast<bool>(unavailable));
    AC_CHECK(
        unavailable.value.failure_reason ==
        workflow::WorkflowFailureReason::NotAvailable);
    AC_CHECK(unavailable.value.game_data_modified == false);
    AC_CHECK(static_cast<bool>(workflow.Cancel(detailed.value.id)));
    AC_CHECK(!static_cast<bool>(workflow.Execute(
        detailed.value.id,
        workflow::ExecutionChoice::Execute,
        detailed.value.bindings)));

    auto failing_adapter = std::make_unique<FakeReadAdapter>();
    failing_adapter->fail = true;
    workflow::OrganizeWorkflowFacade failing_workflow(
        std::move(failing_adapter));
    const auto failed = failing_workflow.RequestPreview(45);
    AC_CHECK(!static_cast<bool>(failed));
    AC_CHECK(failed.code == ErrorCode::CatDataUnavailable);
}

}  // namespace autocattery::tests
