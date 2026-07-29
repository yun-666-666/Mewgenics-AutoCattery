#include "auto_cattery/workflow/state_machine.hpp"

namespace autocattery::workflow {

WorkflowState WorkflowStateMachine::State() const noexcept {
  std::scoped_lock lock(mutex_);
  return state_;
}

bool WorkflowStateMachine::Transition(WorkflowState expected,
                                      WorkflowState next) noexcept {
  std::scoped_lock lock(mutex_);
  if (state_ != expected) {
    return false;
  }
  state_ = next;
  return true;
}

bool WorkflowStateMachine::BeginPreview() noexcept {
  std::scoped_lock lock(mutex_);
  if (state_ != WorkflowState::Idle && state_ != WorkflowState::Completed &&
      state_ != WorkflowState::Failed && state_ != WorkflowState::Cancelled &&
      state_ != WorkflowState::AwaitingConfirmation) {
    return false;
  }
  state_ = WorkflowState::Capturing;
  return true;
}

bool WorkflowStateMachine::ScoringComplete() noexcept {
  return Transition(WorkflowState::Capturing, WorkflowState::Scoring);
}

bool WorkflowStateMachine::PlanningComplete() noexcept {
  std::scoped_lock lock(mutex_);
  if (state_ == WorkflowState::Scoring) {
    state_ = WorkflowState::Planning;
    return true;
  }
  if (state_ == WorkflowState::Planning) {
    state_ = WorkflowState::AwaitingConfirmation;
    return true;
  }
  return false;
}

bool WorkflowStateMachine::BeginApply(WorkflowCapability capability) noexcept {
  if (capability == WorkflowCapability::PreviewOnly) {
    return false;
  }
  return Transition(WorkflowState::AwaitingConfirmation,
                    WorkflowState::Applying);
}

bool WorkflowStateMachine::BeginVerify() noexcept {
  return Transition(WorkflowState::Applying, WorkflowState::Verifying);
}

bool WorkflowStateMachine::Complete() noexcept {
  return Transition(WorkflowState::Verifying, WorkflowState::Completed);
}

void WorkflowStateMachine::Fail() noexcept {
  std::scoped_lock lock(mutex_);
  state_ = WorkflowState::Failed;
}

void WorkflowStateMachine::Cancel() noexcept {
  std::scoped_lock lock(mutex_);
  if (state_ != WorkflowState::Applying && state_ != WorkflowState::Verifying) {
    state_ = WorkflowState::Cancelled;
  }
}

void WorkflowStateMachine::ResetReady() noexcept {
  std::scoped_lock lock(mutex_);
  if (state_ == WorkflowState::Failed || state_ == WorkflowState::Cancelled ||
      state_ == WorkflowState::Completed) {
    state_ = WorkflowState::Idle;
  }
}

} // namespace autocattery::workflow
