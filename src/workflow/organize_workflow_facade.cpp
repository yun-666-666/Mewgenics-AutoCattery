#include "auto_cattery/workflow/organize_workflow_facade.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

#include "auto_cattery/logger.hpp"

namespace autocattery::workflow {

OrganizeWorkflowFacade::OrganizeWorkflowFacade(
    std::unique_ptr<snapshot::IGameReadAdapter> read_adapter)
    : read_adapter_(std::move(read_adapter)) {}

Result<void> OrganizeWorkflowFacade::RequestPreview(
    std::uint64_t scene_generation) {
    if (!read_adapter_) {
        return {
            ErrorCode::NotInitialized,
            "Read-only snapshot adapter is unavailable."
        };
    }
    const auto captured =
        read_adapter_->CaptureHouseSnapshot(scene_generation);
    if (!captured) {
        return {captured.code, captured.message};
    }

    const auto validation = snapshot::Validate(captured.value);
    if (!validation.Valid()) {
        return {
            ErrorCode::SnapshotInvalid,
            "Read-only snapshot failed validation."
        };
    }
    const auto assigned = std::count_if(
        captured.value.cats.begin(),
        captured.value.cats.end(),
        [](const auto& cat) { return cat.room_id.has_value(); });
    std::ostringstream summary;
    summary
        << "Read-only snapshot: house_cats=" << assigned
        << ", rooms=" << captured.value.rooms.size()
        << ", assigned=" << assigned
        << ", warnings=" << validation.WarningCount()
        << "; no data changed.";
    Logger::Instance().Write(
        LogLevel::Info,
        "Snapshot",
        "AC5100",
        summary.str());
    return {
        ErrorCode::Ok,
        summary.str()
    };
}

}  // namespace autocattery::workflow
