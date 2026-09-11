#include "mew_ui_house_move_room_scan.h"

#include <string.h>
#include <windows.h>

static const char* const kKnownRooms[AC_MEW_MOVE_PROBE_ROOM_COUNT] = {
    "Floor1_Large",
    "Floor1_Small",
    "Floor2_Large",
    "Attic",
    "AdventureBox",
    "Floor2_Small"
};

static int AcReadableLiteral(
    const void* pointer,
    const char* literal,
    size_t literal_size) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t start;
    uintptr_t region_end;
    if (!pointer || !literal || literal_size == 0U) {
        return 0;
    }
    start = (uintptr_t)pointer;
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return 0;
    }
    region_end = (uintptr_t)info.BaseAddress +
                 (uintptr_t)info.RegionSize;
    if (region_end < start || literal_size > region_end - start) {
        return 0;
    }
    __try {
        return memcmp(pointer, literal, literal_size) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void AcAppendReference(
    AcMewMoveRoomReference* references,
    uint32_t* count,
    uint32_t offset,
    uint8_t region,
    uint8_t room_index,
    uint8_t pointer_reference) {
    AcMewMoveRoomReference* output;
    if (*count >= AC_MEW_MOVE_PROBE_ROOM_REFERENCES) {
        return;
    }
    output = &references[(*count)++];
    memset(output, 0, sizeof(*output));
    output->offset = offset;
    output->region = region;
    output->room_index = room_index;
    output->pointer_reference = pointer_reference;
}

uint32_t AcMewMoveRoomMask(
    const uint8_t* bytes,
    size_t byte_count) {
    uint32_t mask;
    uint32_t room_index;
    mask = 0U;
    if (!bytes) {
        return mask;
    }
    for (room_index = 0U;
         room_index < AC_MEW_MOVE_PROBE_ROOM_COUNT;
         ++room_index) {
        const char* room = kKnownRooms[room_index];
        const size_t room_size = strlen(room);
        size_t offset;
        if (room_size > byte_count) {
            continue;
        }
        for (offset = 0U; offset + room_size <= byte_count; ++offset) {
            if (memcmp(bytes + offset, room, room_size) == 0) {
                mask |= 1U << room_index;
                break;
            }
        }
    }
    return mask;
}

void AcMewFindMoveRoomReferences(
    const uint8_t* bytes,
    size_t byte_count,
    uint8_t region,
    AcMewMoveRoomReference* references,
    uint32_t* reference_count) {
    uint32_t room_index;
    if (!bytes || !references || !reference_count) {
        return;
    }
    for (room_index = 0U;
         room_index < AC_MEW_MOVE_PROBE_ROOM_COUNT &&
         *reference_count < AC_MEW_MOVE_PROBE_ROOM_REFERENCES;
         ++room_index) {
        const char* room = kKnownRooms[room_index];
        const size_t room_size = strlen(room);
        size_t offset;
        for (offset = 0U; offset + room_size <= byte_count; ++offset) {
            if (memcmp(bytes + offset, room, room_size) == 0) {
                AcAppendReference(
                    references,
                    reference_count,
                    (uint32_t)offset,
                    region,
                    (uint8_t)room_index,
                    0U);
                break;
            }
        }
        for (offset = 0U;
             offset + sizeof(uintptr_t) <= byte_count &&
             *reference_count < AC_MEW_MOVE_PROBE_ROOM_REFERENCES;
             offset += sizeof(uintptr_t)) {
            uintptr_t target;
            memcpy(&target, bytes + offset, sizeof(target));
            if (target > 0xFFFFU &&
                AcReadableLiteral((const void*)target, room, room_size)) {
                AcAppendReference(
                    references,
                    reference_count,
                    (uint32_t)offset,
                    region,
                    (uint8_t)room_index,
                    1U);
            }
        }
    }
}

const char* AcMewMoveProbeRoomId(uint32_t room_index) {
    return room_index < AC_MEW_MOVE_PROBE_ROOM_COUNT
        ? kKnownRooms[room_index]
        : NULL;
}
