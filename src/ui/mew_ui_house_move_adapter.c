#include "mew_ui_house_move_adapter.h"

#include <windows.h>

#include <string.h>

#include "mew_ui_scene_components.h"
#include "mew_ui_house_move_room_scan.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

enum {
    AC_COMPONENT_BUCKET_PREPARE_RVA = 0x963040,
    AC_HOUSE_ROOM_COMPONENT_ID = 0x1D2
};

typedef void (__fastcall *AcPrepareComponentBucketFn)(
    void* component_registry,
    uint32_t component_id);

static int AcReadable(const void* address, size_t size) {
    MEMORY_BASIC_INFORMATION memory;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    if (!address || size == 0U ||
        !VirtualQuery(address, &memory, sizeof(memory))) {
        return 0;
    }
    if (memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return 0;
    }
    start = (uintptr_t)address;
    end = start + size;
    region_end = (uintptr_t)memory.BaseAddress + memory.RegionSize;
    return end >= start && end <= region_end;
}


static int AcTypeEquals(void* component, const char* literal) {
    MewNarrowString type_name;
    const char* data;
    size_t size;
    MewComponent* typed;
    if (!component || !literal) {
        return 0;
    }
    typed = (MewComponent*)component;
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        memset(&type_name, 0, sizeof(type_name));
        typed->vtable->GetObjectTypeSTR(component, &type_name);
        data = MewUI_GetNarrowStringData(&type_name);
        size = MewUI_GetNarrowStringSize(&type_name);
        return data && size == strlen(literal) &&
               memcmp(data, literal, size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void* AcFindHouseComponent(void* scene_manager) {
    MewPodVectorPtr* components;
    uint32_t index;
    components = AcMewGetValidatedSceneComponents(scene_manager);
    if (!components) {
        return NULL;
    }
    for (index = 0U; index < components->size; ++index) {
        if (AcTypeEquals(components->data[index], "House")) {
            return components->data[index];
        }
    }
    return NULL;
}

size_t AcMewEnumerateNativeHouseRooms(
    void* house_scene_manager,
    void** rooms,
    size_t room_capacity) {
    HMODULE executable;
    void* house;
    size_t count;
    size_t index;
    if (!house_scene_manager || !rooms || room_capacity == 0U) {
        return 0U;
    }
    memset(rooms, 0, room_capacity * sizeof(*rooms));
    executable = GetModuleHandleW(NULL);
    house = AcFindHouseComponent(house_scene_manager);
    if (!executable || !house) {
        return 0U;
    }
    __try {
        void* owner = *(void**)((uint8_t*)house + 0x18U);
        void* registry = *(void**)((uint8_t*)owner + 0x08U);
        void* table;
        void* bucket;
        uint32_t bucket_size;
        void** bucket_data;
        ((AcPrepareComponentBucketFn)(
            (uint8_t*)executable +
            AC_COMPONENT_BUCKET_PREPARE_RVA))(
                registry, AC_HOUSE_ROOM_COMPONENT_ID);
        table = *(void**)((uint8_t*)registry + 0x20U);
        bucket = *(void**)((uint8_t*)table +
            AC_HOUSE_ROOM_COMPONENT_ID * 0x10U);
        if (!AcReadable(bucket, 0x18U)) {
            return 0U;
        }
        bucket_size = *(uint32_t*)((uint8_t*)bucket + 0x0CU);
        bucket_data = *(void***)((uint8_t*)bucket + 0x10U);
        count = bucket_size < room_capacity
            ? bucket_size
            : room_capacity;
        if (!AcReadable(
                bucket_data, count * sizeof(*bucket_data))) {
            return 0U;
        }
        for (index = 0U; index < count; ++index) {
            if (!AcReadable(bucket_data[index], sizeof(void*))) {
                return 0U;
            }
            rooms[index] = bucket_data[index];
        }
        return count;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
}

uint32_t AcMewDetectNativeHouseRoomMask(void* room) {
    AcMewMoveRoomReference references[
        AC_MEW_MOVE_PROBE_ROOM_REFERENCES];
    uint32_t reference_count;
    uint32_t mask;
    size_t scan_size;
    MEMORY_BASIC_INFORMATION memory;
    uintptr_t room_start;
    uintptr_t region_end;
    if (!room ||
        VirtualQuery(room, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return 0U;
    }
    room_start = (uintptr_t)room;
    region_end =
        (uintptr_t)memory.BaseAddress + memory.RegionSize;
    if (region_end <= room_start) {
        return 0U;
    }
    scan_size = region_end - room_start;
    if (scan_size > 0x400U) {
        scan_size = 0x400U;
    }
    reference_count = 0U;
    memset(references, 0, sizeof(references));
    __try {
        mask = AcMewMoveRoomMask(
            (const uint8_t*)room, scan_size);
        AcMewFindMoveRoomReferences(
            (const uint8_t*)room,
            scan_size,
            AC_MEW_MOVE_PROBE_COMPONENT,
            references,
            &reference_count);
        while (reference_count > 0U) {
            --reference_count;
            mask |= 1U << references[reference_count].room_index;
        }
        return mask;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
}
