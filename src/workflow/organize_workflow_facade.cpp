#include "auto_cattery/workflow/organize_workflow_facade.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"

namespace autocattery::workflow {

OrganizeWorkflowFacade::OrganizeWorkflowFacade(
    std::unique_ptr<snapshot::IGameReadAdapter> read_adapter,
    Config config,
    WorkflowCapability capability,
    IApprovedTransactionGateway* gateway)
    : read_adapter_(std::move(read_adapter)),
      execution_router_(capability, gateway),
      capability_(capability) {
    if (read_adapter_) {
        preview_builder_ = std::make_unique<PreviewBuilder>(
            *read_adapter_,
            std::move(config));
    }
}

Result<OrganizePreview> OrganizeWorkflowFacade::BuildPreview(
    std::uint64_t scene_generation) {
    if (!preview_builder_) {
        return {
            {},
            ErrorCode::NotInitialized,
            "Read-only snapshot adapter is unavailable."
        };
    }
    if (!state_.BeginPreview()) {
        return {
            {},
            ErrorCode::WriteConflict,
            "An organize workflow is already running."
        };
    }
    auto built = preview_builder_->Build(
        scene_generation,
        capability_,
        state_);
    if (!built) {
        return {{}, built.code, built.message};
    }
    const auto stored = preview_store_.Store(std::move(built.value));
    if (!stored) {
        state_.Fail();
        return {{}, stored.code, stored.message};
    }
    latest_preview_ = stored.value;
    const auto preview = preview_store_.Read(stored.value);
    if (!preview) {
        state_.Fail();
        return {{}, preview.code, preview.message};
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "OrganizeWorkflow",
        "AC11100",
        preview.value.preview.summary);
    return {preview.value.preview};
}

Result<void> OrganizeWorkflowFacade::RequestPreview(
    std::uint64_t scene_generation) {
    const auto preview = BuildPreview(scene_generation);
    return {preview.code, preview ? preview.value.summary : preview.message};
}

WorkflowCapability
OrganizeWorkflowFacade::CurrentExecutionAvailability() const noexcept {
    return capability_;
}

Result<OrganizeOutcome> OrganizeWorkflowFacade::Execute(
    const PreviewId& preview_id,
    ExecutionChoice choice,
    const PreviewBindings& current_bindings) {
    if (capability_ == WorkflowCapability::PreviewOnly) {
        const auto preview = preview_store_.Read(preview_id);
        if (!preview) {
            return {{}, preview.code, preview.message};
        }
        return {
            execution_router_.Execute(preview.value, choice, state_)
        };
    }
    auto claimed = preview_store_.Claim(preview_id, current_bindings);
    if (!claimed) {
        return {{}, claimed.code, claimed.message};
    }
    return {
        execution_router_.Execute(claimed.value, choice, state_)
    };
}

Result<void> OrganizeWorkflowFacade::Cancel(
    const PreviewId& preview_id) {
    const auto cancelled = preview_store_.Cancel(preview_id);
    if (cancelled) {
        state_.Cancel();
    }
    return cancelled;
}

Result<void> OrganizeWorkflowFacade::RequestExecution() {
    if (latest_preview_) {
        const auto preview = preview_store_.Read(*latest_preview_);
        if (preview) {
            const auto outcome = execution_router_.Execute(
                preview.value,
                ExecutionChoice::Execute,
                state_);
            return {
                ErrorCode::UnsupportedGameBuild,
                outcome.message
            };
        }
    }
    return {
        ErrorCode::UnsupportedGameBuild,
        "Preview only: no verified game move or cull adapter is available."
    };
}

WorkflowState OrganizeWorkflowFacade::State() const noexcept {
    return state_.State();
}

}  // namespace autocattery::workflow
