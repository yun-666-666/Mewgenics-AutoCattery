#include "auto_cattery/workflow/state_machine.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWorkflowStateMachineTests() {
  workflow::WorkflowStateMachine state;
  AC_CHECK(state.State() == workflow::WorkflowState::Idle);
  AC_CHECK(state.BeginPreview());
  AC_CHECK(!state.BeginPreview());
  AC_CHECK(state.State() == workflow::WorkflowState::Capturing);
  AC_CHECK(state.ScoringComplete());
  AC_CHECK(state.PlanningComplete());
  AC_CHECK(state.State() == workflow::WorkflowState::Planning);
  AC_CHECK(state.PlanningComplete());
  AC_CHECK(state.State() == workflow::WorkflowState::AwaitingConfirmation);
  AC_CHECK(!state.BeginApply(workflow::WorkflowCapability::PreviewOnly));
  AC_CHECK(state.BeginApply(workflow::WorkflowCapability::MoveOnly));
  AC_CHECK(state.BeginVerify());
  AC_CHECK(state.Complete());

  state.ResetReady();
  AC_CHECK(state.State() == workflow::WorkflowState::Idle);
  AC_CHECK(state.BeginPreview());
  state.Fail();
  AC_CHECK(state.BeginPreview());
  state.Cancel();
  AC_CHECK(state.State() == workflow::WorkflowState::Cancelled);
  AC_CHECK(state.BeginPreview());
}

} // namespace autocattery::tests
