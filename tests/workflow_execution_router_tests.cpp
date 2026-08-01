#include "auto_cattery/workflow/execution_router.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class TransactionGatewayFake final
    : public workflow::IApprovedTransactionGateway {
public:
  execution::ExecutionResult
  ExecuteApproved(const workflow::PreviewBundle &,
                  workflow::ExecutionChoice requested) override {
    ++calls;
    choice = requested;
    return result;
  }

  execution::ExecutionResult result;
  workflow::ExecutionChoice choice{workflow::ExecutionChoice::Execute};
  int calls{};
};

void AwaitConfirmation(workflow::WorkflowStateMachine &state) {
  AC_CHECK(state.BeginPreview());
  AC_CHECK(state.ScoringComplete());
  AC_CHECK(state.PlanningComplete());
  AC_CHECK(state.PlanningComplete());
}

} // namespace

void RunWorkflowExecutionRouterTests() {
  workflow::PreviewBundle bundle;
  bundle.preview.id = "preview";
  TransactionGatewayFake gateway;

  workflow::WorkflowStateMachine preview_state;
  AwaitConfirmation(preview_state);
  workflow::ExecutionRouter preview_only(
      workflow::WorkflowCapability::PreviewOnly, &gateway);
  const auto unavailable = preview_only.Execute(
      bundle, workflow::ExecutionChoice::Execute, preview_state);
  AC_CHECK(gateway.calls == 0);
  AC_CHECK(unavailable.failure_reason ==
           workflow::WorkflowFailureReason::NotAvailable);
  AC_CHECK(unavailable.game_data_modified == false);

  workflow::WorkflowStateMachine move_execute_state;
  AwaitConfirmation(move_execute_state);
  workflow::ExecutionRouter move_only(workflow::WorkflowCapability::MoveOnly,
                                      &gateway);
  const auto cull_blocked = move_only.Execute(
      bundle, workflow::ExecutionChoice::Execute, move_execute_state);
  AC_CHECK(gateway.calls == 0);
  AC_CHECK(cull_blocked.failure_reason ==
           workflow::WorkflowFailureReason::NotAvailable);

  gateway.result.committed = true;
  gateway.result.completed_moves = 2;
  workflow::WorkflowStateMachine move_state;
  AwaitConfirmation(move_state);
  const auto moved = move_only.Execute(
      bundle, workflow::ExecutionChoice::MoveOnly, move_state);
  AC_CHECK(gateway.calls == 1);
  AC_CHECK(gateway.choice == workflow::ExecutionChoice::MoveOnly);
  AC_CHECK(moved.state == workflow::WorkflowState::Completed);
  AC_CHECK(moved.completed_culls == 0);

  gateway.result = {};
  gateway.result.failure_reason = execution::FailureReason::MoveFailed;
  workflow::WorkflowStateMachine failed_state;
  AwaitConfirmation(failed_state);
  const auto failed = move_only.Execute(
      bundle, workflow::ExecutionChoice::MoveOnly, failed_state);
  AC_CHECK(gateway.calls == 2);
  AC_CHECK(failed.state == workflow::WorkflowState::Failed);
  AC_CHECK(failed.completed_culls == 0);

  gateway.result = {};
  gateway.result.failure_reason =
      execution::FailureReason::PreconditionsChanged;
  workflow::WorkflowStateMachine stale_state;
  AwaitConfirmation(stale_state);
  const auto stale = move_only.Execute(
      bundle, workflow::ExecutionChoice::MoveOnly, stale_state);
  AC_CHECK(stale.failure_reason ==
           workflow::WorkflowFailureReason::PreconditionsChanged);
  AC_CHECK(stale.game_data_modified == false);
  AC_CHECK(stale.message.find("click again") != std::string::npos);
}

} // namespace autocattery::tests
