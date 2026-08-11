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

const char* FurnitureWarehouseReplacementStatusName(
    FurnitureWarehouseReplacementStatus status) noexcept {
    switch (status) {
        case FurnitureWarehouseReplacementStatus::Replaced:
            return "replaced";
        case FurnitureWarehouseReplacementStatus::Unsupported:
            return "unsupported";
        case FurnitureWarehouseReplacementStatus::NotFound:
            return "not_found";
        case FurnitureWarehouseReplacementStatus::Ambiguous:
            return "ambiguous";
        case FurnitureWarehouseReplacementStatus::RejectedRestored:
            return "rejected_restored";
        case FurnitureWarehouseReplacementStatus::FailedRestored:
            return "failed_restored";
        case FurnitureWarehouseReplacementStatus::RestoreFailed:
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
    auto same_room = request;
    const auto location = Locate(request.locator);
    if (location.status == FurniturePlacementLookupStatus::Found) {
        same_room.target_room = location.room;
    }
    same_room.strict_target = false;
    return Move(same_room);
}

FurniturePlacementMoveResult FurniturePlacementGateway::Move(
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
    result.from_room = location.room;
    result.target_room = request.target_room;
    if (request.target_room.empty()) {
        result.status = FurniturePlacementMoveStatus::Unsupported;
        result.message = "the target furniture room is empty";
        return result;
    }
    if (location.room == request.target_room &&
        location.saved_x == request.target_x &&
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
    AcMewFurnitureGridSnapshot target_grid{};
    if (location.room == request.target_room) {
        const auto grids = AcMewFindFurnitureGrid(
            house_scene_manager_, request.target_room.c_str());
        if (grids.status != AC_MEW_FURNITURE_FIND_FOUND) {
            result.status = grids.status == AC_MEW_FURNITURE_FIND_NOT_FOUND
                ? FurniturePlacementMoveStatus::NotFound
                : (grids.status == AC_MEW_FURNITURE_FIND_AMBIGUOUS
                    ? FurniturePlacementMoveStatus::Ambiguous
                    : FurniturePlacementMoveStatus::Unsupported);
            result.message = "the current furniture room grid was not uniquely available";
            return result;
        }
        target_grid = grids.snapshot;
    } else {
        const auto grids = AcMewFindFurnitureGrid(
            house_scene_manager_, request.target_room.c_str());
        if (grids.status != AC_MEW_FURNITURE_FIND_FOUND) {
            result.status = grids.status == AC_MEW_FURNITURE_FIND_NOT_FOUND
                ? FurniturePlacementMoveStatus::NotFound
                : (grids.status == AC_MEW_FURNITURE_FIND_AMBIGUOUS
                    ? FurniturePlacementMoveStatus::Ambiguous
                    : FurniturePlacementMoveStatus::Unsupported);
            result.message = "the target furniture room grid was not uniquely available";
            return result;
        }
        target_grid = grids.snapshot;
    }
    const auto moved = AcMewMoveFurnitureToGrid(
        found.snapshot.piece,
        &target_grid,
        request.target_x,
        request.target_y,
        request.strict_target ? 0U : 1U);
    result.target_x = moved.target_x;
    result.target_y = moved.target_y;
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
        result.message =
            moved.target_x == request.target_x &&
                moved.target_y == request.target_y
            ? (location.room == request.target_room
                ? "native furniture validation and commit succeeded"
                : "native cross-room furniture validation and commit succeeded")
            : "native validation selected the closest valid coordinate toward the original placement";
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

FurnitureWarehouseReplacementResult
FurniturePlacementGateway::ReplaceWithWarehouse(
    const FurnitureWarehouseReplacementRequest& request) const {
    FurnitureWarehouseReplacementResult result;
    result.warehouse_item = request.warehouse_item;
    result.warehouse_stable_key = request.warehouse_stable_key;
    result.placed = Locate(request.placed);
    if (result.placed.status != FurniturePlacementLookupStatus::Found) {
        result.status = result.placed.status ==
                FurniturePlacementLookupStatus::NotFound
            ? FurnitureWarehouseReplacementStatus::NotFound
            : (result.placed.status ==
                    FurniturePlacementLookupStatus::Ambiguous
                ? FurnitureWarehouseReplacementStatus::Ambiguous
                : FurnitureWarehouseReplacementStatus::Unsupported);
        result.message = result.placed.message;
        return result;
    }
    if (request.warehouse_item.empty() ||
        request.warehouse_stable_key == 0U ||
        request.warehouse_stable_key == result.placed.stable_key ||
        result.placed.room.empty()) {
        result.message = "the warehouse replacement identity is incomplete";
        return result;
    }
    const auto grid = AcMewFindFurnitureGrid(
        house_scene_manager_, result.placed.room.c_str());
    if (grid.status != AC_MEW_FURNITURE_FIND_FOUND) {
        result.status = grid.status == AC_MEW_FURNITURE_FIND_NOT_FOUND
            ? FurnitureWarehouseReplacementStatus::NotFound
            : (grid.status == AC_MEW_FURNITURE_FIND_AMBIGUOUS
                ? FurnitureWarehouseReplacementStatus::Ambiguous
                : FurnitureWarehouseReplacementStatus::Unsupported);
        result.message = "the placed furniture room grid was not uniquely available";
        return result;
    }
    const auto found = AcMewFindFurniturePiece(
        house_scene_manager_,
        request.placed.item.c_str(),
        request.placed.preferred_key.value_or(0U),
        request.placed.preferred_key.has_value() ? 1 : 0);
    if (found.status != AC_MEW_FURNITURE_FIND_FOUND) {
        const auto refreshed = TranslateLocation(found);
        result.status = refreshed.status ==
                FurniturePlacementLookupStatus::NotFound
            ? FurnitureWarehouseReplacementStatus::NotFound
            : (refreshed.status ==
                    FurniturePlacementLookupStatus::Ambiguous
                ? FurnitureWarehouseReplacementStatus::Ambiguous
                : FurnitureWarehouseReplacementStatus::Unsupported);
        result.message =
            "the furniture scene changed before warehouse replacement: " +
            refreshed.message;
        return result;
    }
    const auto replaced = AcMewReplaceFurnitureWithWarehousePiece(
        house_scene_manager_,
        found.snapshot.piece,
        request.warehouse_stable_key,
        request.warehouse_item.c_str(),
        &grid.snapshot,
        request.target_x.value_or(result.placed.saved_x),
        request.target_y.value_or(result.placed.saved_y));
    result.target_x = replaced.target_x;
    result.target_y = replaced.target_y;
    result.signatures_valid = replaced.signature_valid != 0U;
    result.warehouse_piece_created =
        replaced.warehouse_piece_created != 0U;
    result.placement_valid = replaced.placement_valid != 0U;
    result.committed = replaced.committed != 0U;
    result.verified = replaced.verified != 0U;
    result.rollback_attempted = replaced.rollback_attempted != 0U;
    result.rollback_succeeded = replaced.rollback_succeeded != 0U;
    result.seh_code = replaced.seh_code;
    result.exception_rva = replaced.exception_rva;
    if (result.verified) {
        result.status = FurnitureWarehouseReplacementStatus::Replaced;
        result.message =
            "the warehouse furniture replaced the placed instance through the native scene path";
    } else if (result.rollback_attempted && !result.rollback_succeeded) {
        result.status = FurnitureWarehouseReplacementStatus::RestoreFailed;
        result.message =
            "warehouse replacement failed and the original furniture placement could not be verified";
    } else if (result.rollback_succeeded && !result.placement_valid) {
        result.status =
            FurnitureWarehouseReplacementStatus::RejectedRestored;
        result.message =
            "the warehouse furniture was rejected and the original furniture was restored";
    } else if (result.rollback_succeeded) {
        result.status = FurnitureWarehouseReplacementStatus::FailedRestored;
        result.message =
            "warehouse replacement failed and the original furniture was restored";
    } else {
        result.status = FurnitureWarehouseReplacementStatus::Unsupported;
        result.message =
            "the current build signatures or warehouse furniture identity were not accepted";
    }
    return result;
}

}  // namespace autocattery::ui
