#pragma once

#include "auto_cattery/execution/domain.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"
#include "auto_cattery/workflow/state_machine.hpp"

namespace autocattery::workflow {

class IApprovedTransactionGateway {
public:
  virtual ~IApprovedTransactionGateway() = default;
  virtual execution::ExecutionResult
  ExecuteApproved(const PreviewBundle &bundle, ExecutionChoice choice) = 0;
};

class ExecutionRouter final {
public:
  ExecutionRouter(WorkflowCapability capability,
                  IApprovedTransactionGateway *gateway = nullptr);

  [[nodiscard]] OrganizeOutcome Execute(const PreviewBundle &bundle,
                                        ExecutionChoice choice,
                                        WorkflowStateMachine &state) const;

private:
  WorkflowCapability capability_;
  IApprovedTransactionGateway *gateway_;
};

} // namespace autocattery::workflow
