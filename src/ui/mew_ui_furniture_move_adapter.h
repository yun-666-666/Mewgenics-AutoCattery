#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_FURNITURE_TEXT_CAPACITY 64U

typedef enum AcMewFurnitureFindStatus {
    AC_MEW_FURNITURE_FIND_INVALID = 0,
    AC_MEW_FURNITURE_FIND_FOUND = 1,
    AC_MEW_FURNITURE_FIND_NOT_FOUND = 2,
    AC_MEW_FURNITURE_FIND_AMBIGUOUS = 3
} AcMewFurnitureFindStatus;

typedef struct AcMewFurniturePieceSnapshot {
    void* piece;
    void* grid;
    void* transform;
    void* entry;
    uint64_t stable_key;
    int32_t saved_x;
    int32_t saved_y;
    double world_x;
    double world_y;
    double world_z;
    double scale_x;
    double scale_y;
    double grid_world_x;
    double grid_world_y;
    char item[AC_MEW_FURNITURE_TEXT_CAPACITY];
    char room[AC_MEW_FURNITURE_TEXT_CAPACITY];
} AcMewFurniturePieceSnapshot;

typedef struct AcMewFurnitureGridSnapshot {
    void* grid;
    void* transform;
    double world_x;
    double world_y;
    uint32_t width;
    uint32_t height;
    char room[AC_MEW_FURNITURE_TEXT_CAPACITY];
} AcMewFurnitureGridSnapshot;

typedef struct AcMewFurnitureGridFindResult {
    uint8_t status;
    uint8_t reserved[3];
    uint32_t room_match_count;
    AcMewFurnitureGridSnapshot snapshot;
} AcMewFurnitureGridFindResult;

typedef struct AcMewFurnitureFindResult {
    uint8_t status;
    uint8_t preferred_key_matched;
    uint8_t reserved[2];
    uint32_t item_match_count;
    AcMewFurniturePieceSnapshot snapshot;
} AcMewFurnitureFindResult;

typedef struct AcMewNativeFurnitureMoveResult {
    uint8_t signature_valid;
    uint8_t piece_valid;
    uint8_t grid_valid;
    uint8_t removed;
    uint8_t placement_valid;
    uint8_t committed;
    uint8_t verified;
    uint8_t rollback_attempted;
    uint8_t rollback_succeeded;
    uint8_t reserved[3];
    uint32_t seh_code;
    uintptr_t exception_rva;
    uint64_t stable_key;
    int32_t old_x;
    int32_t old_y;
    int32_t target_x;
    int32_t target_y;
    int32_t committed_x;
    int32_t committed_y;
} AcMewNativeFurnitureMoveResult;

typedef struct AcMewNativeFurnitureReplacementResult {
    uint8_t signature_valid;
    uint8_t placed_piece_valid;
    uint8_t target_grid_valid;
    uint8_t warehouse_piece_created;
    uint8_t old_piece_removed;
    uint8_t placement_valid;
    uint8_t committed;
    uint8_t verified;
    uint8_t old_piece_deleted;
    uint8_t rollback_attempted;
    uint8_t rollback_succeeded;
    uint8_t reserved;
    uint32_t seh_code;
    uintptr_t exception_rva;
    uint64_t placed_stable_key;
    uint64_t warehouse_stable_key;
    int32_t target_x;
    int32_t target_y;
    uint32_t support_dependent_count;
    uint32_t support_dependents_stored;
    uint32_t support_dependents_restored;
} AcMewNativeFurnitureReplacementResult;

typedef struct AcMewNativeWarehousePlacementResult {
    uint8_t signature_valid;
    uint8_t target_grid_valid;
    uint8_t warehouse_piece_created;
    uint8_t placement_valid;
    uint8_t committed;
    uint8_t verified;
    uint8_t rollback_attempted;
    uint8_t rollback_succeeded;
    uint32_t seh_code;
    uintptr_t exception_rva;
    uint64_t warehouse_stable_key;
    int32_t target_x;
    int32_t target_y;
} AcMewNativeWarehousePlacementResult;

typedef struct AcMewFurnitureSupportDependentRequest {
    uint64_t stable_key;
    const char* expected_item;
    int32_t x;
    int32_t y;
} AcMewFurnitureSupportDependentRequest;

typedef struct AcMewFurnitureCoordinate {
    int32_t x;
    int32_t y;
} AcMewFurnitureCoordinate;

size_t AcMewFurnitureCandidatePath(
    int32_t old_x,
    int32_t old_y,
    int32_t target_x,
    int32_t target_y,
    AcMewFurnitureCoordinate* output,
    size_t output_capacity);

size_t AcMewFurnitureNearbyCandidates(
    int32_t target_x,
    int32_t target_y,
    AcMewFurnitureCoordinate* output,
    size_t output_capacity);

double AcMewFurnitureWorldAxis(
    double grid_world_axis,
    int32_t saved_axis,
    double scale_axis);

int AcMewComponentDeleteQueued(void* component);

int AcMewFurnitureDetachedSnapshotMatches(
    const AcMewFurniturePieceSnapshot* snapshot);

void AcMewFurnitureWorldPosition(
    double grid_world_x,
    double grid_world_y,
    int32_t saved_x,
    int32_t saved_y,
    double scale_x,
    double scale_y,
    double* world_x,
    double* world_y,
    double* world_z);

int AcMewReadFurniturePieceSnapshot(
    void* piece,
    AcMewFurniturePieceSnapshot* snapshot);

size_t AcMewEnumerateFurniturePieces(
    void* house_scene_manager,
    AcMewFurniturePieceSnapshot* output,
    size_t output_capacity,
    uint8_t* complete);

size_t AcMewEnumerateFurnitureGrids(
    void* house_scene_manager,
    AcMewFurnitureGridSnapshot* output,
    size_t output_capacity,
    uint8_t* complete);

int AcMewCopyFurnitureGridCells(
    const AcMewFurnitureGridSnapshot* snapshot,
    uint8_t* base_output,
    uint8_t* live_output,
    size_t output_capacity);

AcMewFurnitureGridFindResult AcMewFindFurnitureGrid(
    void* house_scene_manager,
    const char* room);

AcMewFurnitureFindResult AcMewFindFurniturePiece(
    void* house_scene_manager,
    const char* item,
    uint64_t preferred_key,
    int prefer_key);

AcMewNativeFurnitureMoveResult AcMewMoveFurnitureSameRoom(
    void* piece,
    int32_t target_x,
    int32_t target_y);

AcMewNativeFurnitureMoveResult AcMewMoveFurnitureToGrid(
    void* piece,
    const AcMewFurnitureGridSnapshot* target_grid,
    int32_t target_x,
    int32_t target_y,
    uint8_t allow_closest_valid);

AcMewNativeFurnitureReplacementResult
AcMewReplaceFurnitureWithWarehousePiece(
    void* house_scene_manager,
    void* placed_piece,
    uint64_t warehouse_stable_key,
    const char* expected_warehouse_item,
    const AcMewFurnitureGridSnapshot* target_grid,
    int32_t target_x,
    int32_t target_y,
    const AcMewFurnitureSupportDependentRequest* support_dependents_top_down,
    size_t support_dependent_count);

AcMewNativeWarehousePlacementResult AcMewPlaceWarehouseFurniture(
    void* house_scene_manager,
    uint64_t warehouse_stable_key,
    const char* expected_warehouse_item,
    const AcMewFurnitureGridSnapshot* target_grid,
    int32_t target_x,
    int32_t target_y);

AcMewNativeWarehousePlacementResult AcMewStorePlacedFurniture(
    void* house_scene_manager,
    void* placed_piece);

#ifdef __cplusplus
}
#endif
