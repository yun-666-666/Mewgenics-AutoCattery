#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

#include "auto_cattery/config.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/game_read_adapter.hpp"
#include "auto_cattery/workflow/execution_router.hpp"
#include "auto_cattery/workflow/preview_store.hpp"

namespace autocattery::workflow {

class OrganizeWorkflowFacade {
public:
    explicit OrganizeWorkflowFacade(
        std::unique_ptr<snapshot::IGameReadAdapter> read_adapter = {},
        Config config = {},
        WorkflowCapability capability = WorkflowCapability::PreviewOnly,
        IApprovedTransactionGateway* gateway = nullptr,
        std::filesystem::path protection_root = {},
        std::filesystem::path cat_data_root = {});
    virtual ~OrganizeWorkflowFacade() = default;
    [[nodiscard]] virtual Result<OrganizePreview> BuildPreview(
        std::uint64_t scene_generation);
    virtual Result<void> RequestPreview(std::uint64_t scene_generation);
    [[nodiscard]] virtual WorkflowCapability
        CurrentExecutionAvailability() const noexcept;
    [[nodiscard]] virtual Result<OrganizeOutcome> Execute(
        const PreviewId& preview_id,
        ExecutionChoice choice,
        const PreviewBindings& current_bindings);
    virtual Result<void> Cancel(const PreviewId& preview_id);
    virtual Result<void> RequestExecution();
    [[nodiscard]] Result<PreviewBundle> LatestPreview() const;
    [[nodiscard]] Result<void> ApplyConfig(Config config);
    [[nodiscard]] WorkflowState State() const noexcept;

private:
    std::unique_ptr<snapshot::IGameReadAdapter> read_adapter_;
    std::unique_ptr<PreviewBuilder> preview_builder_;
    PreviewStore preview_store_;
    WorkflowStateMachine state_;
    ExecutionRouter execution_router_;
    WorkflowCapability capability_;
    std::filesystem::path protection_root_;
    std::filesystem::path cat_data_root_;
    Config config_;
    mutable std::mutex latest_mutex_;
    std::optional<PreviewId> latest_preview_;
};

}  // namespace autocattery::workflow
