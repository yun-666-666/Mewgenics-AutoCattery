#include "mew_ui_furniture_move_adapter.h"

#include <math.h>
#include <stdlib.h>
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
    AC_FURNITURE_CREATE_PIECE_RVA = 0x1ABFF0,
    AC_FURNITURE_STORE_PIECE_RVA = 0x2EF0D0,
    AC_COMPONENT_DELETE_RVA = 0x94A910,
    AC_FURNITURE_TRANSFORM_OFFSET = 0x38,
    AC_FURNITURE_GRID_OFFSET = 0x48,
    AC_GRID_ROOM_OFFSET = 0x40,
    AC_GRID_LIVE_DATA_OFFSET = 0xE8,
    AC_GRID_WIDTH_OFFSET = 0xF0,
    AC_GRID_HEIGHT_OFFSET = 0xF4,
    AC_GRID_BASE_DATA_OFFSET = 0x108,
    AC_GRID_BASE_WIDTH_OFFSET = 0x110,
    AC_GRID_BASE_HEIGHT_OFFSET = 0x114,
    AC_FURNITURE_ENTRY_OFFSET = 0x2D8,
    AC_COMPONENT_CONTEXT_OFFSET = 0x18,
    AC_COMPONENT_CONTEXT_DELETE_STATE_OFFSET = 0x18,
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
typedef void* (__fastcall *AcFurnitureCreatePieceFn)(
    void* scene_manager,
    void* context,
    const uint64_t* stable_key);
typedef void (__fastcall *AcFurnitureStorePieceFn)(void* piece);
typedef void (__fastcall *AcComponentDeleteFn)(void* component);

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

static int AcAppendUniqueFurnitureCandidate(
    AcMewFurnitureCoordinate* output,
    size_t output_capacity,
    size_t* count,
    int64_t x,
    int64_t y) {
    size_t index;
    if (!output || !count || *count >= output_capacity ||
        x < INT32_MIN || x > INT32_MAX ||
        y < INT32_MIN || y > INT32_MAX) {
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

static int AcAppendFurnitureCandidate(
    AcMewFurnitureCoordinate* output,
    size_t output_capacity,
    size_t* count,
    int32_t old_x,
    int32_t old_y,
    int64_t x,
    int64_t y) {
    if (x == old_x && y == old_y) {
        return 0;
    }
    return AcAppendUniqueFurnitureCandidate(
        output, output_capacity, count, x, y);
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

size_t AcMewFurnitureNearbyCandidates(
    int32_t target_x,
    int32_t target_y,
    AcMewFurnitureCoordinate* output,
    size_t output_capacity) {
    size_t count = 0U;
    int64_t radius;
    if (!output || output_capacity == 0U) {
        return 0U;
    }
    AcAppendUniqueFurnitureCandidate(
        output, output_capacity, &count, target_x, target_y);
    for (radius = 1; radius <= 32 && count < output_capacity; ++radius) {
        int64_t delta_x;
        for (delta_x = -radius;
             delta_x <= radius && count < output_capacity;
             ++delta_x) {
            const int64_t delta_y = radius -
                (delta_x < 0 ? -delta_x : delta_x);
            AcAppendUniqueFurnitureCandidate(
                output,
                output_capacity,
                &count,
                (int64_t)target_x + delta_x,
                (int64_t)target_y + delta_y);
            if (delta_y != 0 && count < output_capacity) {
                AcAppendUniqueFurnitureCandidate(
                    output,
                    output_capacity,
                    &count,
                    (int64_t)target_x + delta_x,
                    (int64_t)target_y - delta_y);
            }
        }
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
    static const uint8_t create_piece_signature[] = {
        0x48, 0x89, 0x5C, 0x24, 0x10,
        0x48, 0x89, 0x6C, 0x24, 0x18,
        0x56, 0x57, 0x41, 0x56,
        0x48, 0x83, 0xEC, 0x20
    };
    static const uint8_t store_piece_signature[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20,
        0x48, 0x8B, 0xD9,
        0xE8, 0xF2, 0xF2, 0xFF, 0xFF
    };
    static const uint8_t delete_component_signature[] = {
        0x40, 0x56, 0x48, 0x83, 0xEC, 0x20,
        0x48, 0x8B, 0x71, 0x18,
        0x48, 0x85, 0xF6
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
            sizeof(commit_signature)) &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_FURNITURE_CREATE_PIECE_RVA,
            create_piece_signature,
            sizeof(create_piece_signature)) &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_FURNITURE_STORE_PIECE_RVA,
            store_piece_signature,
            sizeof(store_piece_signature)) &&
        AcSignatureMatches(
            (const uint8_t*)executable + AC_COMPONENT_DELETE_RVA,
            delete_component_signature,
            sizeof(delete_component_signature));
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

static LONG AcCaptureReplacementException(
    EXCEPTION_POINTERS* exception,
    HMODULE executable,
    AcMewNativeFurnitureReplacementResult* result) {
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

int AcMewComponentDeleteQueued(void* component) {
    void* context;
    if (!component || !AcReadableRange(
            component, AC_COMPONENT_CONTEXT_OFFSET + sizeof(void*))) {
        return 0;
    }
    __try {
        context = *(void**)((uint8_t*)component +
            AC_COMPONENT_CONTEXT_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (!context || !AcReadableRange(
            context,
            AC_COMPONENT_CONTEXT_DELETE_STATE_OFFSET + sizeof(uint8_t))) {
        return 0;
    }
    __try {
        return *(uint8_t*)((uint8_t*)context +
            AC_COMPONENT_CONTEXT_DELETE_STATE_OFFSET) != 0U;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
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
    if (!piece || !snapshot || AcMewComponentDeleteQueued(piece)) {
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

static int AcMewReadFurnitureGridSnapshot(
    void* grid,
    AcMewFurnitureGridSnapshot* snapshot) {
    HMODULE executable;
    void* transform;
    if (!grid || !snapshot) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    executable = GetModuleHandleW(NULL);
    if (!AcValidVtable(grid, executable, AC_FURNITURE_GRID_VTABLE_RVA) ||
        !AcReadableRange(grid, AC_GRID_BASE_HEIGHT_OFFSET + sizeof(uint32_t))) {
        return 0;
    }
    __try {
        transform = *(void**)((uint8_t*)grid + AC_FURNITURE_TRANSFORM_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (!transform ||
        !AcReadableRange(transform, AC_TRANSFORM_Y_OFFSET + sizeof(double))) {
        return 0;
    }
    __try {
        snapshot->grid = grid;
        snapshot->transform = transform;
        snapshot->world_x = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_X_OFFSET);
        snapshot->world_y = *(double*)((uint8_t*)transform +
            AC_TRANSFORM_Y_OFFSET);
        snapshot->width = *(uint32_t*)((uint8_t*)grid +
            AC_GRID_WIDTH_OFFSET);
        snapshot->height = *(uint32_t*)((uint8_t*)grid +
            AC_GRID_HEIGHT_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(snapshot, 0, sizeof(*snapshot));
        return 0;
    }
    if (snapshot->width == 0U || snapshot->height == 0U ||
        snapshot->width > 256U || snapshot->height > 256U ||
        !AcCopyNarrowString(
            (const MewNarrowString*)((uint8_t*)grid + AC_GRID_ROOM_OFFSET),
            snapshot->room,
            sizeof(snapshot->room))) {
        memset(snapshot, 0, sizeof(*snapshot));
        return 0;
    }
    return 1;
}

int AcMewCopyFurnitureGridCells(
    const AcMewFurnitureGridSnapshot* snapshot,
    uint8_t* base_output,
    uint8_t* live_output,
    size_t output_capacity) {
    HMODULE executable;
    uint8_t* base_data;
    uint8_t* live_data;
    uint32_t live_width;
    uint32_t live_height;
    uint32_t base_width;
    uint32_t base_height;
    size_t cell_count;
    if (!snapshot || !snapshot->grid || !base_output || !live_output ||
        snapshot->width == 0U || snapshot->height == 0U) {
        return 0;
    }
    executable = GetModuleHandleW(NULL);
    if (!AcValidVtable(
            snapshot->grid, executable, AC_FURNITURE_GRID_VTABLE_RVA) ||
        !AcReadableRange(
            snapshot->grid,
            AC_GRID_BASE_HEIGHT_OFFSET + sizeof(uint32_t))) {
        return 0;
    }
    __try {
        live_data = *(uint8_t**)((uint8_t*)snapshot->grid +
            AC_GRID_LIVE_DATA_OFFSET);
        live_width = *(uint32_t*)((uint8_t*)snapshot->grid +
            AC_GRID_WIDTH_OFFSET);
        live_height = *(uint32_t*)((uint8_t*)snapshot->grid +
            AC_GRID_HEIGHT_OFFSET);
        base_data = *(uint8_t**)((uint8_t*)snapshot->grid +
            AC_GRID_BASE_DATA_OFFSET);
        base_width = *(uint32_t*)((uint8_t*)snapshot->grid +
            AC_GRID_BASE_WIDTH_OFFSET);
        base_height = *(uint32_t*)((uint8_t*)snapshot->grid +
            AC_GRID_BASE_HEIGHT_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (live_width != snapshot->width || live_height != snapshot->height ||
        base_width != snapshot->width || base_height != snapshot->height ||
        snapshot->width > 256U || snapshot->height > 256U) {
        return 0;
    }
    cell_count = (size_t)snapshot->width * (size_t)snapshot->height;
    if (cell_count == 0U || cell_count > output_capacity ||
        !AcReadableRange(base_data, cell_count) ||
        !AcReadableRange(live_data, cell_count)) {
        return 0;
    }
    __try {
        memcpy(base_output, base_data, cell_count);
        memcpy(live_output, live_data, cell_count);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
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

size_t AcMewEnumerateFurnitureGrids(
    void* house_scene_manager,
    AcMewFurnitureGridSnapshot* output,
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
        AcMewFurnitureGridSnapshot candidate;
        if (!AcMewReadFurnitureGridSnapshot(
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

AcMewFurnitureGridFindResult AcMewFindFurnitureGrid(
    void* house_scene_manager,
    const char* room) {
    AcMewFurnitureGridFindResult result;
    MewPodVectorPtr* components;
    uint32_t index;
    memset(&result, 0, sizeof(result));
    if (!house_scene_manager || !room || room[0] == '\0') {
        result.status = AC_MEW_FURNITURE_FIND_INVALID;
        return result;
    }
    components = AcMewGetValidatedSceneComponents(house_scene_manager);
    if (!components) {
        result.status = AC_MEW_FURNITURE_FIND_INVALID;
        return result;
    }
    for (index = 0U; index < components->size; ++index) {
        AcMewFurnitureGridSnapshot candidate;
        if (!AcMewReadFurnitureGridSnapshot(
                components->data[index], &candidate) ||
            strcmp(candidate.room, room) != 0) {
            continue;
        }
        ++result.room_match_count;
        if (result.room_match_count == 1U) {
            result.snapshot = candidate;
        }
    }
    if (result.room_match_count == 0U) {
        result.status = AC_MEW_FURNITURE_FIND_NOT_FOUND;
    } else if (result.room_match_count == 1U) {
        result.status = AC_MEW_FURNITURE_FIND_FOUND;
    } else {
        result.status = AC_MEW_FURNITURE_FIND_AMBIGUOUS;
    }
    return result;
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
    void* expected_entry,
    void* expected_grid,
    const char* expected_room,
    int32_t expected_x,
    int32_t expected_y) {
    void* entry;
    if (!piece || !expected_entry || !expected_grid || !expected_room ||
        !AcReadableRange(
            piece, AC_FURNITURE_ENTRY_OFFSET + sizeof(void*))) {
        return 0;
    }
    __try {
        entry = *(void**)((uint8_t*)piece + AC_FURNITURE_ENTRY_OFFSET);
        char room[AC_MEW_FURNITURE_TEXT_CAPACITY];
        return *(void**)((uint8_t*)piece + AC_FURNITURE_GRID_OFFSET) ==
                   expected_grid &&
            entry == expected_entry &&
            *(int32_t*)((uint8_t*)entry + AC_ENTRY_SAVED_X_OFFSET) ==
                expected_x &&
            *(int32_t*)((uint8_t*)entry + AC_ENTRY_SAVED_Y_OFFSET) ==
                expected_y &&
            AcCopyNarrowString(
                (const MewNarrowString*)((uint8_t*)entry +
                    AC_ENTRY_ROOM_OFFSET),
                room,
                sizeof(room)) &&
            strcmp(room, expected_room) == 0;
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
            snapshot->entry,
            snapshot->grid,
            snapshot->room,
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
        snapshot->entry,
        snapshot->grid,
        snapshot->room,
        snapshot->saved_x,
        snapshot->saved_y);
}

AcMewNativeFurnitureMoveResult AcMewMoveFurnitureToGrid(
    void* piece,
    const AcMewFurnitureGridSnapshot* target_grid,
    int32_t target_x,
    int32_t target_y,
    uint8_t allow_closest_valid) {
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
    result.grid_valid = (uint8_t)(target_grid && target_grid->grid != NULL);
    if (!result.grid_valid) {
        return result;
    }
    remove_piece = (AcFurnitureRemoveFn)(
        (uint8_t*)executable + AC_FURNITURE_REMOVE_RVA);
    validate_piece = (AcFurnitureValidateFn)(
        (uint8_t*)executable + AC_FURNITURE_VALIDATE_RVA);
    commit_piece = (AcFurnitureCommitFn)(
        (uint8_t*)executable + AC_FURNITURE_COMMIT_RVA);
    if (allow_closest_valid) {
        candidate_count = AcMewFurnitureCandidatePath(
            before.saved_x,
            before.saved_y,
            target_x,
            target_y,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]));
    } else {
        candidates[0].x = target_x;
        candidates[0].y = target_y;
        candidate_count = 1U;
    }
    __try {
        remove_piece(piece);
        result.removed = (uint8_t)(
            *(void**)((uint8_t*)piece + AC_FURNITURE_GRID_OFFSET) == NULL);
        if (result.removed) {
            for (candidate_index = 0U;
                 candidate_index < candidate_count;
                 ++candidate_index) {
                AcMewFurnitureWorldPosition(
                    target_grid->world_x,
                    target_grid->world_y,
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
                    piece, target_grid->grid, 0U);
                if (!result.placement_valid) {
                    continue;
                }
                result.target_x = candidates[candidate_index].x;
                result.target_y = candidates[candidate_index].y;
                commit_piece(piece, target_grid->grid);
                result.committed = 1U;
                result.verified = (uint8_t)AcPlacementMatches(
                    piece,
                    before.entry,
                    target_grid->grid,
                    target_grid->room,
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

AcMewNativeFurnitureMoveResult AcMewMoveFurnitureSameRoom(
    void* piece,
    int32_t target_x,
    int32_t target_y) {
    AcMewFurniturePieceSnapshot before;
    AcMewFurnitureGridSnapshot target;
    memset(&before, 0, sizeof(before));
    memset(&target, 0, sizeof(target));
    if (!AcMewReadFurniturePieceSnapshot(piece, &before) ||
        !AcMewReadFurnitureGridSnapshot(before.grid, &target)) {
        AcMewNativeFurnitureMoveResult result;
        memset(&result, 0, sizeof(result));
        result.target_x = target_x;
        result.target_y = target_y;
        return result;
    }
    return AcMewMoveFurnitureToGrid(
        piece, &target, target_x, target_y, 1U);
}

static int AcSceneContainsFurnitureKey(
    void* house_scene_manager,
    uint64_t stable_key) {
    MewPodVectorPtr* components;
    uint32_t index;
    if (!house_scene_manager || stable_key == 0U) {
        return 0;
    }
    components = AcMewGetValidatedSceneComponents(house_scene_manager);
    if (!components) {
        return 1;
    }
    for (index = 0U; index < components->size; ++index) {
        AcMewFurniturePieceSnapshot snapshot;
        if (AcMewReadFurniturePieceSnapshot(
                components->data[index], &snapshot) &&
            snapshot.stable_key == stable_key) {
            return 1;
        }
    }
    return 0;
}

static int AcStoredEntryMatches(
    const AcMewFurniturePieceSnapshot* before) {
    char room[AC_MEW_FURNITURE_TEXT_CAPACITY];
    if (!before || !before->entry ||
        !AcReadableRange(before->entry, 0x58U)) {
        return 0;
    }
    __try {
        return *(uint64_t*)before->entry == before->stable_key &&
            AcCopyNarrowString(
                (const MewNarrowString*)((uint8_t*)before->entry +
                    AC_ENTRY_ROOM_OFFSET),
                room,
                sizeof(room)) &&
            room[0] == '\0';
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcStoreFurnitureSnapshot(
    const AcMewFurniturePieceSnapshot* before,
    AcFurnitureStorePieceFn store_piece) {
    if (!before || !before->piece || !store_piece) {
        return 0;
    }
    store_piece(before->piece);
    return AcMewComponentDeleteQueued(before->piece) &&
        AcStoredEntryMatches(before) &&
        *(void**)((uint8_t*)before->piece + AC_FURNITURE_ENTRY_OFFSET) ==
            NULL;
}

static int AcPlaceStoredFurniture(
    void* house_scene_manager,
    void* context,
    uint64_t stable_key,
    const char* expected_item,
    void* expected_entry,
    const AcMewFurnitureGridSnapshot* target_grid,
    int32_t target_x,
    int32_t target_y,
    double scale_x,
    double scale_y,
    AcFurnitureCreatePieceFn create_piece,
    AcFurnitureValidateFn validate_piece,
    AcFurnitureCommitFn commit_piece,
    AcFurnitureStorePieceFn store_piece,
    void** placed_piece,
    AcMewFurniturePieceSnapshot* placed_snapshot,
    uint8_t* created_valid,
    uint8_t* placement_valid,
    uint8_t* committed) {
    void* piece;
    AcMewFurniturePieceSnapshot snapshot;
    double world_x;
    double world_y;
    double world_z;
    if (placed_piece) {
        *placed_piece = NULL;
    }
    if (placed_snapshot) {
        memset(placed_snapshot, 0, sizeof(*placed_snapshot));
    }
    if (created_valid) {
        *created_valid = 0U;
    }
    if (placement_valid) {
        *placement_valid = 0U;
    }
    if (committed) {
        *committed = 0U;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    if (!house_scene_manager || !context || stable_key == 0U ||
        !expected_item || expected_item[0] == '\0' || !target_grid ||
        !target_grid->grid || !create_piece || !validate_piece ||
        !commit_piece || !store_piece ||
        AcSceneContainsFurnitureKey(house_scene_manager, stable_key)) {
        return 0;
    }
    piece = create_piece(house_scene_manager, context, &stable_key);
    if (!piece || !AcMewReadFurniturePieceSnapshot(piece, &snapshot) ||
        snapshot.stable_key != stable_key || snapshot.grid != NULL ||
        snapshot.room[0] != '\0' ||
        strcmp(snapshot.item, expected_item) != 0 ||
        (expected_entry && snapshot.entry != expected_entry)) {
        return 0;
    }
    if (created_valid) {
        *created_valid = 1U;
    }
    *(double*)((uint8_t*)snapshot.transform +
        AC_TRANSFORM_SCALE_X_OFFSET) = scale_x;
    *(double*)((uint8_t*)snapshot.transform +
        AC_TRANSFORM_SCALE_Y_OFFSET) = scale_y;
    AcMewFurnitureWorldPosition(
        target_grid->world_x,
        target_grid->world_y,
        target_x,
        target_y,
        scale_x,
        scale_y,
        &world_x,
        &world_y,
        &world_z);
    *(double*)((uint8_t*)snapshot.transform + AC_TRANSFORM_X_OFFSET) =
        world_x;
    *(double*)((uint8_t*)snapshot.transform + AC_TRANSFORM_Y_OFFSET) =
        world_y;
    *(double*)((uint8_t*)snapshot.transform + AC_TRANSFORM_Z_OFFSET) =
        world_z;
    if (!validate_piece(piece, target_grid->grid, 0U)) {
        store_piece(piece);
        return 0;
    }
    if (placement_valid) {
        *placement_valid = 1U;
    }
    commit_piece(piece, target_grid->grid);
    if (committed) {
        *committed = 1U;
    }
    if (!AcPlacementMatches(
            piece,
            snapshot.entry,
            target_grid->grid,
            target_grid->room,
            target_x,
            target_y)) {
        store_piece(piece);
        return 0;
    }
    if (placed_piece) {
        *placed_piece = piece;
    }
    if (placed_snapshot) {
        *placed_snapshot = snapshot;
    }
    return 1;
}

static int AcSceneMatchesOriginalPlacement(
    void* house_scene_manager,
    const AcMewFurniturePieceSnapshot* before) {
    AcMewFurnitureFindResult found;
    if (!house_scene_manager || !before) {
        return 0;
    }
    found = AcMewFindFurniturePiece(
        house_scene_manager,
        before->item,
        before->stable_key,
        1);
    return found.status == AC_MEW_FURNITURE_FIND_FOUND &&
        found.preferred_key_matched &&
        AcPlacementMatches(
            found.snapshot.piece,
            before->entry,
            before->grid,
            before->room,
            before->saved_x,
            before->saved_y);
}

typedef struct AcMewFurnitureTransactionDependent {
    AcMewFurnitureSupportDependentRequest request;
    AcMewFurniturePieceSnapshot before;
    void* restored_piece;
    AcMewFurniturePieceSnapshot restored_snapshot;
} AcMewFurnitureTransactionDependent;

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
    size_t support_dependent_count) {
    AcMewNativeFurnitureReplacementResult result;
    AcMewFurniturePieceSnapshot placed_before;
    AcMewFurniturePieceSnapshot warehouse_piece;
    AcMewFurnitureGridSnapshot verified_grid;
    AcMewFurnitureTransactionDependent* dependents;
    HMODULE executable;
    void* context;
    void* created_piece;
    AcFurnitureValidateFn validate_piece;
    AcFurnitureCommitFn commit_piece;
    AcFurnitureCreatePieceFn create_piece;
    AcFurnitureStorePieceFn store_piece;
    size_t index;
    size_t stored_count;
    size_t restored_count;
    int old_stored;
    int new_placed;
    int transaction_ok;
    memset(&result, 0, sizeof(result));
    memset(&placed_before, 0, sizeof(placed_before));
    memset(&warehouse_piece, 0, sizeof(warehouse_piece));
    memset(&verified_grid, 0, sizeof(verified_grid));
    result.warehouse_stable_key = warehouse_stable_key;
    result.target_x = target_x;
    result.target_y = target_y;
    if (support_dependent_count > UINT32_MAX ||
        (support_dependent_count != 0U &&
         (!support_dependents_top_down ||
          support_dependent_count >
              SIZE_MAX / sizeof(AcMewFurnitureTransactionDependent)))) {
        return result;
    }
    result.support_dependent_count =
        (uint32_t)support_dependent_count;
    executable = GetModuleHandleW(NULL);
    result.signature_valid =
        (uint8_t)AcNativeSignaturesMatch(executable);
    result.placed_piece_valid = (uint8_t)(
        AcMewReadFurniturePieceSnapshot(
            placed_piece, &placed_before) &&
        placed_before.grid != NULL);
    if (result.placed_piece_valid) {
        result.placed_stable_key = placed_before.stable_key;
    }
    result.target_grid_valid = (uint8_t)(
        target_grid && target_grid->grid &&
        AcMewReadFurnitureGridSnapshot(
            target_grid->grid, &verified_grid) &&
        strcmp(verified_grid.room, target_grid->room) == 0);
    if (!result.signature_valid || !result.placed_piece_valid ||
        !result.target_grid_valid || !house_scene_manager ||
        warehouse_stable_key == 0U ||
        warehouse_stable_key == placed_before.stable_key ||
        !expected_warehouse_item || expected_warehouse_item[0] == '\0' ||
        AcSceneContainsFurnitureKey(
            house_scene_manager, warehouse_stable_key)) {
        return result;
    }
    context = MewUI_GetContextFromScene(house_scene_manager);
    if (!context) {
        return result;
    }
    validate_piece = (AcFurnitureValidateFn)(
        (uint8_t*)executable + AC_FURNITURE_VALIDATE_RVA);
    commit_piece = (AcFurnitureCommitFn)(
        (uint8_t*)executable + AC_FURNITURE_COMMIT_RVA);
    create_piece = (AcFurnitureCreatePieceFn)(
        (uint8_t*)executable + AC_FURNITURE_CREATE_PIECE_RVA);
    store_piece = (AcFurnitureStorePieceFn)(
        (uint8_t*)executable + AC_FURNITURE_STORE_PIECE_RVA);
    dependents = support_dependent_count == 0U
        ? NULL
        : (AcMewFurnitureTransactionDependent*)calloc(
            support_dependent_count,
            sizeof(AcMewFurnitureTransactionDependent));
    if (support_dependent_count != 0U && !dependents) {
        return result;
    }
    for (index = 0U; index < support_dependent_count; ++index) {
        AcMewFurnitureFindResult found;
        size_t duplicate;
        dependents[index].request = support_dependents_top_down[index];
        if (dependents[index].request.stable_key == 0U ||
            dependents[index].request.stable_key ==
                placed_before.stable_key ||
            dependents[index].request.stable_key == warehouse_stable_key ||
            !dependents[index].request.expected_item ||
            dependents[index].request.expected_item[0] == '\0') {
            free(dependents);
            return result;
        }
        for (duplicate = 0U; duplicate < index; ++duplicate) {
            if (dependents[duplicate].request.stable_key ==
                dependents[index].request.stable_key) {
                free(dependents);
                return result;
            }
        }
        found = AcMewFindFurniturePiece(
            house_scene_manager,
            dependents[index].request.expected_item,
            dependents[index].request.stable_key,
            1);
        if (found.status != AC_MEW_FURNITURE_FIND_FOUND ||
            !found.preferred_key_matched ||
            found.snapshot.grid != verified_grid.grid ||
            strcmp(found.snapshot.room, verified_grid.room) != 0 ||
            found.snapshot.saved_x != dependents[index].request.x ||
            found.snapshot.saved_y != dependents[index].request.y) {
            free(dependents);
            return result;
        }
        dependents[index].before = found.snapshot;
    }
    created_piece = NULL;
    stored_count = 0U;
    restored_count = 0U;
    old_stored = 0;
    new_placed = 0;
    transaction_ok = 1;
    __try {
        for (index = 0U; index < support_dependent_count; ++index) {
            if (!AcStoreFurnitureSnapshot(
                    &dependents[index].before, store_piece)) {
                transaction_ok = 0;
                break;
            }
            ++stored_count;
            result.support_dependents_stored = (uint32_t)stored_count;
        }
        if (transaction_ok) {
            old_stored = AcStoreFurnitureSnapshot(
                &placed_before, store_piece);
            result.old_piece_removed = (uint8_t)old_stored;
            result.old_piece_deleted = (uint8_t)old_stored;
            transaction_ok = old_stored;
        }
        if (transaction_ok) {
            new_placed = AcPlaceStoredFurniture(
                house_scene_manager,
                context,
                warehouse_stable_key,
                expected_warehouse_item,
                NULL,
                &verified_grid,
                target_x,
                target_y,
                placed_before.scale_x,
                placed_before.scale_y,
                create_piece,
                validate_piece,
                commit_piece,
                store_piece,
                &created_piece,
                &warehouse_piece,
                &result.warehouse_piece_created,
                &result.placement_valid,
                &result.committed);
            transaction_ok = new_placed;
        }
        if (transaction_ok) {
            for (index = support_dependent_count;
                 index > 0U;
                 --index) {
                AcMewFurnitureTransactionDependent* dependent =
                    &dependents[index - 1U];
                uint8_t created_valid;
                uint8_t placement_valid;
                uint8_t committed;
                if (!AcPlaceStoredFurniture(
                        house_scene_manager,
                        context,
                        dependent->before.stable_key,
                        dependent->before.item,
                        dependent->before.entry,
                        &verified_grid,
                        dependent->before.saved_x,
                        dependent->before.saved_y,
                        dependent->before.scale_x,
                        dependent->before.scale_y,
                        create_piece,
                        validate_piece,
                        commit_piece,
                        store_piece,
                        &dependent->restored_piece,
                        &dependent->restored_snapshot,
                        &created_valid,
                        &placement_valid,
                        &committed)) {
                    transaction_ok = 0;
                    break;
                }
                ++restored_count;
                result.support_dependents_restored =
                    (uint32_t)restored_count;
            }
        }
        if (transaction_ok) {
            result.verified = 1U;
        }
    }
    __except (AcCaptureReplacementException(
        GetExceptionInformation(), executable, &result)) {
        transaction_ok = 0;
    }
    if (result.verified) {
        free(dependents);
        return result;
    }
    if (stored_count != 0U || old_stored || new_placed ||
        restored_count != 0U || result.seh_code != 0U) {
        int rollback_ok = 1;
        result.rollback_attempted = 1U;
        __try {
            if (restored_count != 0U) {
                const size_t first_restored =
                    support_dependent_count - restored_count;
                for (index = first_restored;
                     index < support_dependent_count;
                     ++index) {
                    if (!AcStoreFurnitureSnapshot(
                            &dependents[index].restored_snapshot,
                            store_piece)) {
                        rollback_ok = 0;
                        break;
                    }
                }
            }
            if (rollback_ok && new_placed &&
                !AcStoreFurnitureSnapshot(&warehouse_piece, store_piece)) {
                rollback_ok = 0;
            }
            if (rollback_ok && old_stored) {
                void* restored_old;
                AcMewFurniturePieceSnapshot restored_snapshot;
                uint8_t created_valid;
                uint8_t placement_valid;
                uint8_t committed;
                rollback_ok = AcPlaceStoredFurniture(
                    house_scene_manager,
                    context,
                    placed_before.stable_key,
                    placed_before.item,
                    placed_before.entry,
                    &verified_grid,
                    placed_before.saved_x,
                    placed_before.saved_y,
                    placed_before.scale_x,
                    placed_before.scale_y,
                    create_piece,
                    validate_piece,
                    commit_piece,
                    store_piece,
                    &restored_old,
                    &restored_snapshot,
                    &created_valid,
                    &placement_valid,
                    &committed);
            }
            if (rollback_ok) {
                for (index = stored_count; index > 0U; --index) {
                    AcMewFurnitureTransactionDependent* dependent =
                        &dependents[index - 1U];
                    void* restored_piece;
                    AcMewFurniturePieceSnapshot restored_snapshot;
                    uint8_t created_valid;
                    uint8_t placement_valid;
                    uint8_t committed;
                    if (!AcPlaceStoredFurniture(
                            house_scene_manager,
                            context,
                            dependent->before.stable_key,
                            dependent->before.item,
                            dependent->before.entry,
                            &verified_grid,
                            dependent->before.saved_x,
                            dependent->before.saved_y,
                            dependent->before.scale_x,
                            dependent->before.scale_y,
                            create_piece,
                            validate_piece,
                            commit_piece,
                            store_piece,
                            &restored_piece,
                            &restored_snapshot,
                            &created_valid,
                            &placement_valid,
                            &committed)) {
                        rollback_ok = 0;
                        break;
                    }
                }
            }
            if (rollback_ok &&
                !AcSceneMatchesOriginalPlacement(
                    house_scene_manager, &placed_before)) {
                rollback_ok = 0;
            }
            if (rollback_ok) {
                for (index = 0U;
                     index < support_dependent_count;
                     ++index) {
                    if (!AcSceneMatchesOriginalPlacement(
                            house_scene_manager,
                            &dependents[index].before)) {
                        rollback_ok = 0;
                        break;
                    }
                }
            }
            result.rollback_succeeded = (uint8_t)rollback_ok;
        }
        __except (AcCaptureReplacementException(
            GetExceptionInformation(), executable, &result)) {
            result.rollback_succeeded = 0U;
        }
    }
    free(dependents);
    return result;
}
