#include "mew_ui_furniture_move_adapter.h"

#include <math.h>
#include <string.h>
#include <windows.h>

#include "mew_ui_scene_components.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

enum {
    AC_FURNITURE_PIECE_VTABLE_RVA = 0xEDE690,
    AC_FURNITURE_GRID_VTABLE_RVA = 0xEF4C20,
    AC_FURNITURE_REMOVE_RVA = 0x2EE3D0,
    AC_FURNITURE_VALIDATE_RVA = 0x2EDE60,
    AC_FURNITURE_COMMIT_RVA = 0x2EE230,
    AC_FURNITURE_TRANSFORM_OFFSET = 0x38,
    AC_FURNITURE_GRID_OFFSET = 0x48,
    AC_FURNITURE_ENTRY_OFFSET = 0x2D8,
    AC_ENTRY_ITEM_OFFSET = 0x08,
    AC_ENTRY_ROOM_OFFSET = 0x30,
    AC_ENTRY_SAVED_X_OFFSET = 0x50,
    AC_ENTRY_SAVED_Y_OFFSET = 0x54,
    AC_TRANSFORM_X_OFFSET = 0x80,
    AC_TRANSFORM_Y_OFFSET = 0x88,
    AC_TRANSFORM_Z_OFFSET = 0x90,
    AC_TRANSFORM_SCALE_X_OFFSET = 0xB8,
    AC_TRANSFORM_SCALE_Y_OFFSET = 0xC0
};

typedef void (__fastcall *AcFurnitureRemoveFn)(void* piece);
typedef uint8_t (__fastcall *AcFurnitureValidateFn)(
    void* piece,
    void* grid,
    uint8_t alternate_rules);
typedef void (__fastcall *AcFurnitureCommitFn)(void* piece, void* grid);

static int AcReadableRange(const void* pointer, size_t byte_count) {
    uintptr_t current;
    uintptr_t end;
    if (!pointer || byte_count == 0U) {
        return 0;
    }
    current = (uintptr_t)pointer;
    if (byte_count > UINTPTR_MAX - current) {
        return 0;
    }
    end = current + byte_count;
    while (current < end) {
        MEMORY_BASIC_INFORMATION memory;
        uintptr_t region_end;
        if (VirtualQuery(
                (const void*)current,
                &memory,
                sizeof(memory)) != sizeof(memory) ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            return 0;
        }
        region_end = (uintptr_t)memory.BaseAddress + memory.RegionSize;
        if (region_end <= current) {
            return 0;
        }
        current = region_end < end ? region_end : end;
    }
    return 1;
}

static int AcCopyNarrowString(
    const MewNarrowString* value,
    char* output,
    size_t output_capacity) {
    const char* data;
    size_t size;
    if (!value || !output || output_capacity == 0U ||
        !AcReadableRange(value, sizeof(*value))) {
        return 0;
    }
    __try {
        size = MewUI_GetNarrowStringSize(value);
        data = MewUI_GetNarrowStringData(value);
        if (!data || size >= output_capacity ||
            size > value->capacity ||
            !AcReadableRange(data, size == 0U ? 1U : size)) {
            return 0;
        }
        if (size != 0U) {
            memcpy(output, data, size);
        }
        output[size] = '\0';
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcSupportedScale(double value) {
    return value == 1.0 || value == -1.0;
}

static int AcAppendFurnitureCandidate(
    AcMewFurnitureCoordinate* output,
    size_t output_capacity,
    size_t* count,
    int32_t old_x,
    int32_t old_y,
    int64_t x,
    int64_t y) {
    size_t index;
    if (!output || !count || *count >= output_capacity ||
        x < INT32_MIN || x > INT32_MAX ||
        y < INT32_MIN || y > INT32_MAX ||
        (x == old_x && y == old_y)) {
        return 0;
    }
    for (index = 0U; index < *count; ++index) {
        if (output[index].x == (int32_t)x &&
            output[index].y == (int32_t)y) {
            return 0;
        }
    }
    output[*count].x = (int32_t)x;
    output[*count].y = (int32_t)y;
    ++*count;
    return 1;
}

size_t AcMewFurnitureCandidatePath(
    int32_t old_x,
    int32_t old_y,
    int32_t target_x,
    int32_t target_y,
    AcMewFurnitureCoordinate* output,
    size_t output_capacity) {
    size_t count = 0U;
    int32_t x = target_x;
    int32_t y = target_y;
    size_t step;
    if (!output || output_capacity == 0U) {
        return 0U;
    }
    AcAppendFurnitureCandidate(
        output, output_capacity, &count,
        old_x, old_y, target_x, target_y);
    for (step = 0U; step < 64U && count < output_capacity; ++step) {
        if (x == old_x && y == old_y) {
            break;
        }
        if (x < old_x) {
            ++x;
        } else if (x > old_x) {
            --x;
        } else if (y < old_y) {
            ++y;
        } else if (y > old_y) {
            --y;
        }
        AcAppendFurnitureCandidate(
            output, output_capacity, &count, old_x, old_y, x, y);
        AcAppendFurnitureCandidate(
            output, output_capacity, &count, old_x, old_y, x, (int64_t)y - 1);
        AcAppendFurnitureCandidate(
            output, output_capacity, &count, old_x, old_y, (int64_t)x - 1, y);
        AcAppendFurnitureCandidate(
            output, output_capacity, &count, old_x, old_y, (int64_t)x + 1, y);
        AcAppendFurnitureCandidate(
            output, output_capacity, &count, old_x, old_y, x, (int64_t)y + 1);
    }
    return count;
}

static int AcSignatureMatches(
    const uint8_t* address,
    const uint8_t* expected,
    size_t expected_size) {
    return AcReadableRange(address, expected_size) &&
        memcmp(address, expected, expected_size) == 0;
}

static int AcNativeSignaturesMatch(HMODULE executable) {
    static const uint8_t remove_signature[] = {
        0x40, 0x55, 0x57, 0x48, 0x83, 0xEC, 0x28,
        0x48, 0x8B, 0x79, 0x48, 0x48, 0x8B, 0xE9
    };
    static const uint8_t validate_signature[] = {
        0x40, 0x53, 0x57, 0x41, 0x55, 0x41, 0x57,
        0x48, 0x83, 0xEC, 0x38, 0x45, 0x33, 0xDB,
        0x45, 0x0F, 0xB6, 0xE8
    };
    static const uint8_t commit_signature[] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x6C, 0x24, 0x18,
        0x48, 0x89, 0x74, 0x24, 0x20,
        0x57, 0x48, 0x83, 0xEC, 0x50
    };
    return executable &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_FURNITURE_REMOVE_RVA,
            remove_signature,
            sizeof(remove_signature)) &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_FURNITURE_VALIDATE_RVA,
            validate_signature,
            sizeof(validate_signature)) &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_FURNITURE_COMMIT_RVA,
            commit_signature,
            sizeof(commit_signature));
}

static int AcValidVtable(
    const void* object,
    HMODULE executable,
    uintptr_t expected_rva) {
    return executable && AcReadableRange(object, sizeof(void*)) &&
        *(void* const*)object ==
            (const uint8_t*)executable + expected_rva;
}

static uint32_t AcImageSize(HMODULE executable) {
    IMAGE_DOS_HEADER* dos;
    IMAGE_NT_HEADERS64* nt;
    if (!executable) {
        return 0U;
    }
    dos = (IMAGE_DOS_HEADER*)executable;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0U;
    }
    nt = (IMAGE_NT_HEADERS64*)((uint8_t*)executable + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return 0U;
    }
    return nt->OptionalHeader.SizeOfImage;
}

static LONG AcCaptureException(
    EXCEPTION_POINTERS* exception,
    HMODULE executable,
    AcMewNativeFurnitureMoveResult* result) {
    if (exception && exception->ExceptionRecord && result) {
        const uintptr_t address =
            (uintptr_t)exception->ExceptionRecord->ExceptionAddress;
        const uintptr_t base = (uintptr_t)executable;
        const uint32_t image_size = AcImageSize(executable);
        result->seh_code = exception->ExceptionRecord->ExceptionCode;
        result->exception_rva =
            address >= base && address - base < image_size
                ? address - base
                : 0U;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

double AcMewFurnitureWorldAxis(
    double grid_world_axis,
    int32_t saved_axis,
    double scale_axis) {
    return grid_world_axis + (double)saved_axis + ceil(11.5 * scale_axis);
}

void AcMewFurnitureWorldPosition(
    double grid_world_x,
    double grid_world_y,
    int32_t saved_x,
    int32_t saved_y,
    double scale_x,
    double scale_y,
    double* world_x,
    double* world_y,
    double* world_z) {
    if (world_x) {
        *world_x = AcMewFurnitureWorldAxis(
            grid_world_x, saved_x, scale_x);
    }
    if (world_y) {
        *world_y = AcMewFurnitureWorldAxis(
            grid_world_y, saved_y, scale_y);
    }
    if (world_z) {
        *world_z = 0.0;
    }
}

int AcMewReadFurniturePieceSnapshot(
    void* piece,
    AcMewFurniturePieceSnapshot* snapshot) {
    HMODULE executable;
    void* transform;
    void* grid;
    void* grid_transform;
    void* entry;
    if (!piece || !snapshot) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    executable = GetModuleHandleW(NULL);
    if (!AcValidVtable(
            piece, executable, AC_FURNITURE_PIECE_VTABLE_RVA) ||
        !AcReadableRange(
            piece, AC_FURNITURE_ENTRY_OFFSET + sizeof(void*))) {
        return 0;
    }
    __try {
        transform = *(void**)((uint8_t*)piece +
            AC_FURNITURE_TRANSFORM_OFFSET);
        grid = *(void**)((uint8_t*)piece + AC_FURNITURE_GRID_OFFSET);
        entry = *(void**)((uint8_t*)piece + AC_FURNITURE_ENTRY_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (!transform || !entry ||
        !AcReadableRange(
            transform, AC_TRANSFORM_SCALE_Y_OFFSET + sizeof(double)) ||
        !AcReadableRange(entry, 0x68U)) {
        return 0;
    }
    grid_transform = NULL;
    if (grid) {
        if (!AcValidVtable(
                grid, executable, AC_FURNITURE_GRID_VTABLE_RVA) ||
            !AcReadableRange(grid, 0xF8U)) {
            return 0;
        }
        __try {
            grid_transform = *(void**)((uint8_t*)grid +
                AC_FURNITURE_TRANSFORM_OFFSET);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
        if (!grid_transform ||
            !AcReadableRange(
                grid_transform, AC_TRANSFORM_Y_OFFSET + sizeof(double))) {
            return 0;
        }
    }
    __try {
        snapshot->piece = piece;
        snapshot->grid = grid;
        snapshot->transform = transform;
        snapshot->entry = entry;
        snapshot->stable_key = *(uint64_t*)entry;
        snapshot->saved_x = *(int32_t*)((uint8_t*)entry +
            AC_ENTRY_SAVED_X_OFFSET);
        snapshot->saved_y = *(int32_t*)((uint8_t*)entry +
            AC_ENTRY_SAVED_Y_OFFSET);
        snapshot->world_x = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_X_OFFSET);
        snapshot->world_y = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_Y_OFFSET);
        snapshot->world_z = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_Z_OFFSET);
        snapshot->scale_x = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_SCALE_X_OFFSET);
        snapshot->scale_y = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_SCALE_Y_OFFSET);
        if (grid_transform) {
            snapshot->grid_world_x = *(double*)((uint8_t*)grid_transform +
                AC_TRANSFORM_X_OFFSET);
            snapshot->grid_world_y = *(double*)((uint8_t*)grid_transform +
                AC_TRANSFORM_Y_OFFSET);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(snapshot, 0, sizeof(*snapshot));
        return 0;
    }
    if (!AcSupportedScale(snapshot->scale_x) ||
        !AcSupportedScale(snapshot->scale_y) ||
        !AcCopyNarrowString(
            (const MewNarrowString*)((uint8_t*)entry +
                AC_ENTRY_ITEM_OFFSET),
            snapshot->item,
            sizeof(snapshot->item)) ||
        !AcCopyNarrowString(
            (const MewNarrowString*)((uint8_t*)entry +
                AC_ENTRY_ROOM_OFFSET),
            snapshot->room,
            sizeof(snapshot->room))) {
        memset(snapshot, 0, sizeof(*snapshot));
        return 0;
    }
    return 1;
}

size_t AcMewEnumerateFurniturePieces(
    void* house_scene_manager,
    AcMewFurniturePieceSnapshot* output,
    size_t output_capacity,
    uint8_t* complete) {
    MewPodVectorPtr* components;
    size_t count = 0U;
    uint32_t index;
    if (complete) {
        *complete = 0U;
    }
    if (!house_scene_manager || !output || output_capacity == 0U ||
        !complete) {
        return 0U;
    }
    components = AcMewGetValidatedSceneComponents(house_scene_manager);
    if (!components) {
        return 0U;
    }
    *complete = 1U;
    for (index = 0U; index < components->size; ++index) {
        AcMewFurniturePieceSnapshot candidate;
        if (!AcMewReadFurniturePieceSnapshot(
                components->data[index], &candidate)) {
            continue;
        }
        if (count == output_capacity) {
            *complete = 0U;
            continue;
        }
        output[count++] = candidate;
    }
    return count;
}

AcMewFurnitureFindResult AcMewFindFurniturePiece(
    void* house_scene_manager,
    const char* item,
    uint64_t preferred_key,
    int prefer_key) {
    AcMewFurnitureFindResult result;
    AcMewFurniturePieceSnapshot fallback;
    MewPodVectorPtr* components;
    uint32_t index;
    memset(&result, 0, sizeof(result));
    memset(&fallback, 0, sizeof(fallback));
    if (!house_scene_manager || !item || item[0] == '\0') {
        result.status = AC_MEW_FURNITURE_FIND_INVALID;
        return result;
    }
    components = AcMewGetValidatedSceneComponents(house_scene_manager);
    if (!components) {
        result.status = AC_MEW_FURNITURE_FIND_INVALID;
        return result;
    }
    for (index = 0U; index < components->size; ++index) {
        AcMewFurniturePieceSnapshot candidate;
        if (!AcMewReadFurniturePieceSnapshot(
                components->data[index], &candidate) ||
            strcmp(candidate.item, item) != 0) {
            continue;
        }
        ++result.item_match_count;
        if (result.item_match_count == 1U) {
            fallback = candidate;
        }
        if (prefer_key && candidate.stable_key == preferred_key) {
            result.status = AC_MEW_FURNITURE_FIND_FOUND;
            result.preferred_key_matched = 1U;
            result.snapshot = candidate;
            return result;
        }
    }
    if (result.item_match_count == 0U) {
        result.status = AC_MEW_FURNITURE_FIND_NOT_FOUND;
    } else if (result.item_match_count == 1U) {
        result.status = AC_MEW_FURNITURE_FIND_FOUND;
        result.snapshot = fallback;
    } else {
        result.status = AC_MEW_FURNITURE_FIND_AMBIGUOUS;
    }
    return result;
}

static int AcPlacementMatches(
    void* piece,
    const AcMewFurniturePieceSnapshot* snapshot,
    int32_t expected_x,
    int32_t expected_y) {
    void* entry;
    if (!piece || !snapshot ||
        !AcReadableRange(
            piece, AC_FURNITURE_ENTRY_OFFSET + sizeof(void*))) {
        return 0;
    }
    __try {
        entry = *(void**)((uint8_t*)piece + AC_FURNITURE_ENTRY_OFFSET);
        return *(void**)((uint8_t*)piece + AC_FURNITURE_GRID_OFFSET) ==
                   snapshot->grid &&
            entry == snapshot->entry &&
            *(int32_t*)((uint8_t*)entry + AC_ENTRY_SAVED_X_OFFSET) ==
                expected_x &&
            *(int32_t*)((uint8_t*)entry + AC_ENTRY_SAVED_Y_OFFSET) ==
                expected_y;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcSnapshotTransformMatches(
    const AcMewFurniturePieceSnapshot* snapshot) {
    if (!snapshot || !snapshot->transform ||
        !AcReadableRange(
            snapshot->transform,
            AC_TRANSFORM_SCALE_Y_OFFSET + sizeof(double))) {
        return 0;
    }
    __try {
        return *(double*)((uint8_t*)snapshot->transform +
                   AC_TRANSFORM_X_OFFSET) == snapshot->world_x &&
            *(double*)((uint8_t*)snapshot->transform +
                   AC_TRANSFORM_Y_OFFSET) == snapshot->world_y &&
            *(double*)((uint8_t*)snapshot->transform +
                   AC_TRANSFORM_Z_OFFSET) == snapshot->world_z &&
            *(double*)((uint8_t*)snapshot->transform +
                   AC_TRANSFORM_SCALE_X_OFFSET) == snapshot->scale_x &&
            *(double*)((uint8_t*)snapshot->transform +
                   AC_TRANSFORM_SCALE_Y_OFFSET) == snapshot->scale_y;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcRestoreFurniturePlacement(
    const AcMewFurniturePieceSnapshot* snapshot,
    AcFurnitureRemoveFn remove_piece,
    AcFurnitureCommitFn commit_piece,
    int force_recommit) {
    void* current_grid;
    if (!force_recommit &&
        AcPlacementMatches(
            snapshot->piece,
            snapshot,
            snapshot->saved_x,
            snapshot->saved_y) &&
        AcSnapshotTransformMatches(snapshot)) {
        return 1;
    }
    current_grid = *(void**)((uint8_t*)snapshot->piece +
        AC_FURNITURE_GRID_OFFSET);
    if (current_grid) {
        remove_piece(snapshot->piece);
    }
    *(double*)((uint8_t*)snapshot->transform +
        AC_TRANSFORM_X_OFFSET) = snapshot->world_x;
    *(double*)((uint8_t*)snapshot->transform +
        AC_TRANSFORM_Y_OFFSET) = snapshot->world_y;
    *(double*)((uint8_t*)snapshot->transform +
        AC_TRANSFORM_Z_OFFSET) = snapshot->world_z;
    *(double*)((uint8_t*)snapshot->transform +
        AC_TRANSFORM_SCALE_X_OFFSET) = snapshot->scale_x;
    *(double*)((uint8_t*)snapshot->transform +
        AC_TRANSFORM_SCALE_Y_OFFSET) = snapshot->scale_y;
    commit_piece(snapshot->piece, snapshot->grid);
    return AcPlacementMatches(
        snapshot->piece,
        snapshot,
        snapshot->saved_x,
        snapshot->saved_y);
}

AcMewNativeFurnitureMoveResult AcMewMoveFurnitureSameRoom(
    void* piece,
    int32_t target_x,
    int32_t target_y) {
    AcMewNativeFurnitureMoveResult result;
    AcMewFurniturePieceSnapshot before;
    HMODULE executable;
    AcFurnitureRemoveFn remove_piece;
    AcFurnitureValidateFn validate_piece;
    AcFurnitureCommitFn commit_piece;
    double target_world_x;
    double target_world_y;
    double target_world_z;
    AcMewFurnitureCoordinate candidates[256];
    size_t candidate_count;
    size_t candidate_index;
    memset(&result, 0, sizeof(result));
    memset(&before, 0, sizeof(before));
    result.target_x = target_x;
    result.target_y = target_y;
    executable = GetModuleHandleW(NULL);
    result.signature_valid =
        (uint8_t)AcNativeSignaturesMatch(executable);
    result.piece_valid =
        (uint8_t)AcMewReadFurniturePieceSnapshot(piece, &before);
    if (!result.signature_valid || !result.piece_valid) {
        return result;
    }
    result.stable_key = before.stable_key;
    result.old_x = before.saved_x;
    result.old_y = before.saved_y;
    result.grid_valid = (uint8_t)(before.grid != NULL);
    if (!result.grid_valid) {
        return result;
    }
    remove_piece = (AcFurnitureRemoveFn)(
        (uint8_t*)executable + AC_FURNITURE_REMOVE_RVA);
    validate_piece = (AcFurnitureValidateFn)(
        (uint8_t*)executable + AC_FURNITURE_VALIDATE_RVA);
    commit_piece = (AcFurnitureCommitFn)(
        (uint8_t*)executable + AC_FURNITURE_COMMIT_RVA);
    candidate_count = AcMewFurnitureCandidatePath(
        before.saved_x,
        before.saved_y,
        target_x,
        target_y,
        candidates,
        sizeof(candidates) / sizeof(candidates[0]));
    __try {
        remove_piece(piece);
        result.removed = (uint8_t)(
            *(void**)((uint8_t*)piece + AC_FURNITURE_GRID_OFFSET) == NULL);
        if (result.removed) {
            for (candidate_index = 0U;
                 candidate_index < candidate_count;
                 ++candidate_index) {
                AcMewFurnitureWorldPosition(
                    before.grid_world_x,
                    before.grid_world_y,
                    candidates[candidate_index].x,
                    candidates[candidate_index].y,
                    before.scale_x,
                    before.scale_y,
                    &target_world_x,
                    &target_world_y,
                    &target_world_z);
                *(double*)((uint8_t*)before.transform +
                    AC_TRANSFORM_X_OFFSET) = target_world_x;
                *(double*)((uint8_t*)before.transform +
                    AC_TRANSFORM_Y_OFFSET) = target_world_y;
                *(double*)((uint8_t*)before.transform +
                    AC_TRANSFORM_Z_OFFSET) = target_world_z;
                result.placement_valid = validate_piece(
                    piece, before.grid, 0U);
                if (!result.placement_valid) {
                    continue;
                }
                result.target_x = candidates[candidate_index].x;
                result.target_y = candidates[candidate_index].y;
                commit_piece(piece, before.grid);
                result.committed = 1U;
                result.verified = (uint8_t)AcPlacementMatches(
                    piece,
                    &before,
                    result.target_x,
                    result.target_y);
                break;
            }
        }
    }
    __except (AcCaptureException(
        GetExceptionInformation(), executable, &result)) {}
    __try {
        if (before.entry && AcReadableRange(before.entry, 0x58U)) {
            result.committed_x = *(int32_t*)((uint8_t*)before.entry +
                AC_ENTRY_SAVED_X_OFFSET);
            result.committed_y = *(int32_t*)((uint8_t*)before.entry +
                AC_ENTRY_SAVED_Y_OFFSET);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (result.verified) {
        return result;
    }
    if (result.removed || result.committed || result.seh_code != 0U) {
        result.rollback_attempted = 1U;
        __try {
            result.rollback_succeeded = (uint8_t)AcRestoreFurniturePlacement(
                &before,
                remove_piece,
                commit_piece,
                result.seh_code != 0U);
        }
        __except (AcCaptureException(
            GetExceptionInformation(), executable, &result)) {
            result.rollback_succeeded = 0U;
        }
    }
    return result;
}
