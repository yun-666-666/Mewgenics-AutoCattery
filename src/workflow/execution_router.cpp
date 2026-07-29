#include "auto_cattery/workflow/execution_router.hpp"

namespace autocattery::workflow {
namespace {

OrganizeOutcome NotAvailable(const PreviewBundle &bundle,
                             WorkflowCapability capability,
                             std::string message) {
  return {bundle.preview.id,
          capability,
          WorkflowState::AwaitingConfirmation,
          WorkflowFailureReason::NotAvailable,
          0,
          0,
          false,
          false,
          std::move(message)};
}

} // namespace

ExecutionRouter::ExecutionRouter(WorkflowCapability capability,
                                 IApprovedTransactionGateway *gateway)
    : capability_(capability), gateway_(gateway) {}

OrganizeOutcome ExecutionRouter::Execute(const PreviewBundle &bundle,
                                         ExecutionChoice choice,
                                         WorkflowStateMachine &state) const {
  if (capability_ == WorkflowCapability::PreviewOnly || gateway_ == nullptr) {
    return NotAvailable(
        bundle, capability_,
        "Read-only mode: no verified game write adapter is available.");
  }
  if (capability_ == WorkflowCapability::MoveOnly &&
      choice == ExecutionChoice::Execute) {
    return NotAvailable(bundle, capability_,
                        "Cull execution is unavailable; choose move-only.");
  }
  if (!state.BeginApply(capability_)) {
    return {bundle.preview.id,
            capability_,
            state.State(),
            WorkflowFailureReason::Busy,
            0,
            0,
            false,
            false,
            "The workflow is already running."};
  }

  const auto result = gateway_->ExecuteApproved(bundle, choice);
  if (!result.committed) {
    state.Fail();
    return {bundle.preview.id,
            capability_,
            WorkflowState::Failed,
            WorkflowFailureReason::ExecutionFailed,
            result.completed_moves,
            result.completed_culls,
            result.completed_moves != 0 || result.completed_culls != 0,
            false,
            "Execution did not commit; no cull fallback was attempted."};
  }
  if (!state.BeginVerify() || !state.Complete()) {
    state.Fail();
    return {bundle.preview.id,
            capability_,
            WorkflowState::Failed,
            WorkflowFailureReason::InvalidTransition,
            result.completed_moves,
            result.completed_culls,
            true,
            false,
            "Execution committed but workflow state verification failed."};
  }
  return {bundle.preview.id,
          capability_,
          WorkflowState::Completed,
          WorkflowFailureReason::None,
          result.completed_moves,
          result.completed_culls,
          true,
          false,
          "Organize transaction committed."};
}

} // namespace autocattery::workflow
