#include "auto_cattery/workflow/organize_workflow_facade.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"
#include "auto_cattery/diagnostics/cat_data_collector.hpp"
#include "auto_cattery/protection/policy.hpp"

namespace autocattery::workflow {
namespace {

std::filesystem::path ProtectionPath(
    const std::filesystem::path& root,
    const Config& config) {
    return root.empty()
        ? std::filesystem::path{}
        : root / config.protection.sidecar_file;
}

}  // namespace

OrganizeWorkflowFacade::OrganizeWorkflowFacade(
    std::unique_ptr<snapshot::IGameReadAdapter> read_adapter,
    Config config,
    WorkflowCapability capability,
    IApprovedTransactionGateway* gateway,
    std::filesystem::path protection_root,
    std::filesystem::path cat_data_root)
    : read_adapter_(std::move(read_adapter)),
      execution_router_(capability, gateway),
      capability_(capability),
      protection_root_(std::move(protection_root)),
      cat_data_root_(std::move(cat_data_root)),
      config_(config) {
    if (read_adapter_) {
        preview_builder_ = std::make_unique<PreviewBuilder>(
            *read_adapter_,
            config,
            ProtectionPath(protection_root_, config));
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
    if (config_.diagnostics.collect_cat_data && !cat_data_root_.empty()) {
        const auto collected = diagnostics::WriteCatDataSnapshot(
            built.value, cat_data_root_);
        Logger::Instance().Write(
            collected ? LogLevel::Info : LogLevel::Warn,
            "CatDataCollector",
            collected ? "AC19000" : "AC19001",
            collected
                ? "Opt-in cat data snapshot saved without names, paths, or account data."
                : "Opt-in cat data snapshot failed: " + collected.message);
    }
    const auto stored = preview_store_.Store(std::move(built.value));
    if (!stored) {
        state_.Fail();
        return {{}, stored.code, stored.message};
    }
    {
        std::scoped_lock lock(latest_mutex_);
        latest_preview_ = stored.value;
    }
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

Result<OrganizeOutcome>
OrganizeWorkflowFacade::RequestExecutionOutcome() {
    std::optional<PreviewId> latest;
    {
        std::scoped_lock lock(latest_mutex_);
        latest = latest_preview_;
    }
    if (latest) {
        const auto preview = preview_store_.Read(*latest);
        if (preview) {
            const auto current_protection =
                preview_builder_->CaptureProtectionDigest(
                    preview.value.snapshot, capability_);
            if (current_protection !=
                protection::BuildDigest(preview.value.protections)) {
                const auto cancel_result = preview_store_.Cancel(*latest);
                (void)cancel_result;
                state_.Cancel();
                return {
                    {},
                    ErrorCode::OperationCancelled,
                    "Protection rules changed; click again to preview."
                };
            }
            const auto outcome = execution_router_.Execute(
                preview.value,
                capability_ == WorkflowCapability::MoveOnly
                    ? ExecutionChoice::MoveOnly
                    : ExecutionChoice::Execute,
                state_);
            return {
                outcome,
                outcome.failure_reason == WorkflowFailureReason::None
                    ? ErrorCode::Ok
                    : outcome.failure_reason ==
                            WorkflowFailureReason::PreconditionsChanged
                        ? ErrorCode::OperationCancelled
                        : ErrorCode::UnsupportedGameBuild,
                outcome.message
            };
        }
    }
    return {
        {},
        ErrorCode::UnsupportedGameBuild,
        "Preview only: no verified game move or cull adapter is available."
    };
}

Result<void> OrganizeWorkflowFacade::RequestExecution() {
    const auto outcome = RequestExecutionOutcome();
    return {outcome.code, outcome.message};
}

Result<PreviewBundle> OrganizeWorkflowFacade::LatestPreview() const {
    std::optional<PreviewId> latest;
    {
        std::scoped_lock lock(latest_mutex_);
        latest = latest_preview_;
    }
    if (!latest) {
        return {{}, ErrorCode::OperationCancelled,
                "no organize preview is available"};
    }
    return preview_store_.Read(*latest);
}

Result<room_planning::RoomPlan>
OrganizeWorkflowFacade::BuildFurnitureRoomPurposes(
    std::uint64_t scene_generation) {
    if (!preview_builder_) {
        return {
            {}, ErrorCode::NotInitialized,
            "Read-only snapshot adapter is unavailable."
        };
    }
    WorkflowStateMachine state;
    if (!state.BeginPreview()) {
        return {{}, ErrorCode::WriteConflict,
                "Furniture room-purpose analysis is already running."};
    }
    auto built = preview_builder_->Build(
        scene_generation,
        WorkflowCapability::MoveOnly,
        state,
        true);
    if (!built) {
        return {{}, built.code, built.message};
    }
    return {std::move(built.value.room_plan)};
}

Result<void> OrganizeWorkflowFacade::ApplyConfig(Config config) {
    if (state_.State() != WorkflowState::Idle) {
        return {
            ErrorCode::WriteConflict,
            "configuration can only be applied while the workflow is idle"
        };
    }
    if (!read_adapter_) {
        return {
            ErrorCode::NotInitialized,
            "read-only snapshot adapter is unavailable"
        };
    }
    preview_builder_ = std::make_unique<PreviewBuilder>(
        *read_adapter_,
        config,
        ProtectionPath(protection_root_, config));
    config_ = config;
    preview_store_.InvalidateAll();
    {
        std::scoped_lock lock(latest_mutex_);
        latest_preview_.reset();
    }
    return {};
}

WorkflowState OrganizeWorkflowFacade::State() const noexcept {
    return state_.State();
}

}  // namespace autocattery::workflow
