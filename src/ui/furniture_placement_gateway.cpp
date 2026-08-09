#include "furniture_placement_gateway.hpp"

#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "mew_ui_furniture_move_adapter.h"

namespace autocattery::ui {
namespace {

FurniturePlacementLocation TranslateLocation(
    const AcMewFurnitureFindResult& found) {
    FurniturePlacementLocation result;
    if (found.status == AC_MEW_FURNITURE_FIND_FOUND) {
        result.status = FurniturePlacementLookupStatus::Found;
        result.stable_key = found.snapshot.stable_key;
        result.saved_x = found.snapshot.saved_x;
        result.saved_y = found.snapshot.saved_y;
        result.item = found.snapshot.item;
        result.room = found.snapshot.room;
        result.message = found.preferred_key_matched
            ? "matched the preferred furniture instance key"
            : "matched the only furniture instance with this item id";
    } else if (found.status == AC_MEW_FURNITURE_FIND_NOT_FOUND) {
        result.status = FurniturePlacementLookupStatus::NotFound;
        result.message = "the requested furniture item is not present in the active House scene";
    } else if (found.status == AC_MEW_FURNITURE_FIND_AMBIGUOUS) {
        result.status = FurniturePlacementLookupStatus::Ambiguous;
        result.message = "multiple furniture instances matched and the preferred key was absent";
    } else {
        result.status = FurniturePlacementLookupStatus::Unsupported;
        result.message = "the active House component list is unavailable";
    }
    return result;
}

FurniturePlacementMoveStatus LookupFailureStatus(
    FurniturePlacementLookupStatus status) {
    switch (status) {
        case FurniturePlacementLookupStatus::NotFound:
            return FurniturePlacementMoveStatus::NotFound;
        case FurniturePlacementLookupStatus::Ambiguous:
            return FurniturePlacementMoveStatus::Ambiguous;
        default:
            return FurniturePlacementMoveStatus::Unsupported;
    }
}

}  // namespace

const char* FurniturePlacementMoveStatusName(
    FurniturePlacementMoveStatus status) noexcept {
    switch (status) {
        case FurniturePlacementMoveStatus::Moved:
            return "moved";
        case FurniturePlacementMoveStatus::AlreadyPlaced:
            return "already_placed";
        case FurniturePlacementMoveStatus::Unsupported:
            return "unsupported";
        case FurniturePlacementMoveStatus::NotFound:
            return "not_found";
        case FurniturePlacementMoveStatus::Ambiguous:
            return "ambiguous";
        case FurniturePlacementMoveStatus::RejectedRestored:
            return "rejected_restored";
        case FurniturePlacementMoveStatus::FailedRestored:
            return "failed_restored";
        case FurniturePlacementMoveStatus::RestoreFailed:
            return "restore_failed";
    }
    return "unsupported";
}

bool FurniturePlacementGateway::Initialize(
    const std::filesystem::path& game_executable) {
    house_scene_manager_ = nullptr;
    build_supported_ = static_cast<bool>(
        save_safety::CurrentMewgenicsBuildGate().Verify(game_executable));
    return build_supported_;
}

void FurniturePlacementGateway::SetHouseScene(
    void* house_scene_manager) noexcept {
    house_scene_manager_ = house_scene_manager;
}

FurniturePlacementLocation FurniturePlacementGateway::Locate(
    const FurniturePlacementLocator& locator) const {
    if (!build_supported_ || !house_scene_manager_ || locator.item.empty()) {
        FurniturePlacementLocation result;
        result.status = FurniturePlacementLookupStatus::Unsupported;
        result.message = "native furniture placement is unavailable in the current scene";
        return result;
    }
    return TranslateLocation(AcMewFindFurniturePiece(
        house_scene_manager_,
        locator.item.c_str(),
        locator.preferred_key.value_or(0U),
        locator.preferred_key.has_value() ? 1 : 0));
}

FurniturePlacementMoveResult FurniturePlacementGateway::MoveSameRoom(
    const FurniturePlacementRequest& request) const {
    FurniturePlacementMoveResult result;
    result.target_x = request.target_x;
    result.target_y = request.target_y;
    const auto location = Locate(request.locator);
    if (location.status != FurniturePlacementLookupStatus::Found) {
        result.status = LookupFailureStatus(location.status);
        result.message = location.message;
        return result;
    }
    result.stable_key = location.stable_key;
    result.from_x = location.saved_x;
    result.from_y = location.saved_y;
    if (location.saved_x == request.target_x &&
        location.saved_y == request.target_y) {
        result.status = FurniturePlacementMoveStatus::AlreadyPlaced;
        result.verified = true;
        result.message = "the furniture is already at the requested saved coordinate";
        return result;
    }
    const auto found = AcMewFindFurniturePiece(
        house_scene_manager_,
        request.locator.item.c_str(),
        request.locator.preferred_key.value_or(0U),
        request.locator.preferred_key.has_value() ? 1 : 0);
    if (found.status != AC_MEW_FURNITURE_FIND_FOUND) {
        const auto refreshed = TranslateLocation(found);
        result.status = LookupFailureStatus(refreshed.status);
        result.message = "the furniture scene changed before native movement: " +
            refreshed.message;
        return result;
    }
    const auto moved = AcMewMoveFurnitureSameRoom(
        found.snapshot.piece, request.target_x, request.target_y);
    result.signatures_valid = moved.signature_valid != 0U;
    result.placement_valid = moved.placement_valid != 0U;
    result.committed = moved.committed != 0U;
    result.verified = moved.verified != 0U;
    result.rollback_attempted = moved.rollback_attempted != 0U;
    result.rollback_succeeded = moved.rollback_succeeded != 0U;
    result.seh_code = moved.seh_code;
    result.exception_rva = moved.exception_rva;
    if (result.verified) {
        result.status = FurniturePlacementMoveStatus::Moved;
        result.message = "native furniture validation and commit succeeded";
    } else if (result.rollback_attempted && !result.rollback_succeeded) {
        result.status = FurniturePlacementMoveStatus::RestoreFailed;
        result.message = "native furniture movement failed and the original placement could not be verified";
    } else if (result.rollback_succeeded && !result.placement_valid) {
        result.status = FurniturePlacementMoveStatus::RejectedRestored;
        result.message = "the requested coordinate was rejected and the original placement was restored";
    } else if (result.rollback_succeeded) {
        result.status = FurniturePlacementMoveStatus::FailedRestored;
        result.message = "native furniture movement failed and the original placement was restored";
    } else {
        result.status = FurniturePlacementMoveStatus::Unsupported;
        result.message = "the current build signatures or furniture pointers were not accepted";
    }
    return result;
}

}  // namespace autocattery::ui
