#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace autocattery::ui {

enum class FurniturePlacementLookupStatus {
    Found,
    Unsupported,
    NotFound,
    Ambiguous
};

struct FurniturePlacementLocator {
    std::string item;
    std::optional<std::uint64_t> preferred_key;
};

struct FurniturePlacementLocation {
    FurniturePlacementLookupStatus status{
        FurniturePlacementLookupStatus::Unsupported};
    std::uint64_t stable_key{};
    std::int32_t saved_x{};
    std::int32_t saved_y{};
    std::string item;
    std::string room;
    std::string message;
};

enum class FurniturePlacementMoveStatus {
    Moved,
    AlreadyPlaced,
    Unsupported,
    NotFound,
    Ambiguous,
    RejectedRestored,
    FailedRestored,
    RestoreFailed
};

struct FurniturePlacementRequest {
    FurniturePlacementLocator locator;
    std::string target_room;
    std::int32_t target_x{};
    std::int32_t target_y{};
    bool strict_target{};
};

struct FurniturePlacementMoveResult {
    FurniturePlacementMoveStatus status{
        FurniturePlacementMoveStatus::Unsupported};
    std::uint64_t stable_key{};
    std::int32_t from_x{};
    std::int32_t from_y{};
    std::int32_t target_x{};
    std::int32_t target_y{};
    std::string from_room;
    std::string target_room;
    bool signatures_valid{};
    bool placement_valid{};
    bool committed{};
    bool verified{};
    bool rollback_attempted{};
    bool rollback_succeeded{};
    std::uint32_t seh_code{};
    std::uintptr_t exception_rva{};
    std::string message;
};

enum class FurnitureWarehouseReplacementStatus {
    Replaced,
    Unsupported,
    NotFound,
    Ambiguous,
    RejectedRestored,
    FailedRestored,
    RestoreFailed
};

struct FurnitureWarehouseReplacementRequest {
    FurniturePlacementLocator placed;
    std::string warehouse_item;
    std::uint64_t warehouse_stable_key{};
    std::optional<std::int32_t> target_x;
    std::optional<std::int32_t> target_y;
};

struct FurnitureWarehouseReplacementResult {
    FurnitureWarehouseReplacementStatus status{
        FurnitureWarehouseReplacementStatus::Unsupported};
    FurniturePlacementLocation placed;
    std::uint64_t warehouse_stable_key{};
    std::string warehouse_item;
    std::int32_t target_x{};
    std::int32_t target_y{};
    bool signatures_valid{};
    bool warehouse_piece_created{};
    bool placement_valid{};
    bool committed{};
    bool verified{};
    bool rollback_attempted{};
    bool rollback_succeeded{};
    std::uint32_t seh_code{};
    std::uintptr_t exception_rva{};
    std::string message;
};

[[nodiscard]] const char* FurniturePlacementMoveStatusName(
    FurniturePlacementMoveStatus status) noexcept;
[[nodiscard]] const char* FurnitureWarehouseReplacementStatusName(
    FurnitureWarehouseReplacementStatus status) noexcept;

class FurniturePlacementGateway {
public:
    [[nodiscard]] bool Initialize(
        const std::filesystem::path& game_executable);
    void SetHouseScene(void* house_scene_manager) noexcept;

    [[nodiscard]] FurniturePlacementLocation Locate(
        const FurniturePlacementLocator& locator) const;
    [[nodiscard]] FurniturePlacementMoveResult MoveSameRoom(
        const FurniturePlacementRequest& request) const;
    [[nodiscard]] FurniturePlacementMoveResult Move(
        const FurniturePlacementRequest& request) const;
    [[nodiscard]] FurnitureWarehouseReplacementResult ReplaceWithWarehouse(
        const FurnitureWarehouseReplacementRequest& request) const;

private:
    bool build_supported_{};
    void* house_scene_manager_{};
};

}  // namespace autocattery::ui
