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
    std::int32_t target_x{};
    std::int32_t target_y{};
};

struct FurniturePlacementMoveResult {
    FurniturePlacementMoveStatus status{
        FurniturePlacementMoveStatus::Unsupported};
    std::uint64_t stable_key{};
    std::int32_t from_x{};
    std::int32_t from_y{};
    std::int32_t target_x{};
    std::int32_t target_y{};
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

[[nodiscard]] const char* FurniturePlacementMoveStatusName(
    FurniturePlacementMoveStatus status) noexcept;

class FurniturePlacementGateway {
public:
    [[nodiscard]] bool Initialize(
        const std::filesystem::path& game_executable);
    void SetHouseScene(void* house_scene_manager) noexcept;

    [[nodiscard]] FurniturePlacementLocation Locate(
        const FurniturePlacementLocator& locator) const;
    [[nodiscard]] FurniturePlacementMoveResult MoveSameRoom(
        const FurniturePlacementRequest& request) const;

private:
    bool build_supported_{};
    void* house_scene_manager_{};
};

}  // namespace autocattery::ui
