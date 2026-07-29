#pragma once

#include <mutex>

#include "auto_cattery/workflow/domain.hpp"

namespace autocattery::workflow {

class WorkflowStateMachine final {
public:
  [[nodiscard]] WorkflowState State() const noexcept;
  [[nodiscard]] bool BeginPreview() noexcept;
  [[nodiscard]] bool ScoringComplete() noexcept;
  [[nodiscard]] bool PlanningComplete() noexcept;
  [[nodiscard]] bool BeginApply(WorkflowCapability capability) noexcept;
  [[nodiscard]] bool BeginVerify() noexcept;
  [[nodiscard]] bool Complete() noexcept;
  void Fail() noexcept;
  void Cancel() noexcept;
  void ResetReady() noexcept;

private:
  bool Transition(WorkflowState expected, WorkflowState next) noexcept;

  mutable std::mutex mutex_;
  WorkflowState state_{WorkflowState::Idle};
};

} // namespace autocattery::workflow
