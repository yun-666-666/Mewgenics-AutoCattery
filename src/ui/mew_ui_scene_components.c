#include "mew_ui_scene_components.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

static int AcMewReadableRange(const void* pointer, size_t byte_count) {
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
        MEMORY_BASIC_INFORMATION info;
        uintptr_t region_end;
        const DWORD blocked = PAGE_GUARD | PAGE_NOACCESS;
        if (VirtualQuery(
                (const void*)current,
                &info,
                sizeof(info)) != sizeof(info) ||
            info.State != MEM_COMMIT ||
            (info.Protect & blocked) != 0U) {
            return 0;
        }
        region_end = (uintptr_t)info.BaseAddress +
                     (uintptr_t)info.RegionSize;
        if (region_end <= current) {
            return 0;
        }
        current = region_end < end ? region_end : end;
    }
    return 1;
}

static int AcMewExecutableAddress(const void* pointer) {
    MEMORY_BASIC_INFORMATION info;
    DWORD protection;
    if (!pointer ||
        VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return 0;
    }
    protection = info.Protect & 0xFFU;
    return protection == PAGE_EXECUTE ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

static int AcMewIsDisplayContainer(void** vtable) {
    /* FindChildByName calls slots 3 and 0 before checking the object type.
       Executable slots alone also accept unrelated objects and destructors. */
    static const char* const names[] = {
        ".?AVMovieClip@swf@glaiel@@",
        ".?AVDisplayObjectContainer@swf@glaiel@@",
        ".?AVDisplayObjectContainer_DynamicBatched@swf@glaiel@@"
    };
    const uintptr_t image = (uintptr_t)GetModuleHandleW(NULL);
    MEMORY_BASIC_INFORMATION info;
    size_t index;
    __try {
        const uint32_t* locator;
        const char* name;
        if (!vtable || VirtualQuery(vtable, &info, sizeof(info)) != sizeof(info) ||
            info.Type != MEM_IMAGE || (uintptr_t)info.AllocationBase != image ||
            !AcMewReadableRange(vtable - 1, sizeof(void*))) {
            return 0;
        }
        locator = (const uint32_t*)vtable[-1];
        if (!AcMewReadableRange(locator, 6U * sizeof(uint32_t)) ||
            locator[0] != 1U || locator[1] != 0U ||
            (uintptr_t)locator != image + locator[5]) {
            return 0;
        }
        name = (const char*)(image + locator[3] + 2U * sizeof(void*));
        for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
            const size_t length = strlen(names[index]) + 1U;
            if (AcMewReadableRange(name, length) &&
                memcmp(name, names[index], length) == 0) {
                return 1;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

MewPodVectorPtr* AcMewGetValidatedSceneComponents(void* scene_manager) {
    MewPodVectorPtr* components;
    size_t data_size;
    if (!scene_manager) {
        return NULL;
    }
    __try {
        components = *(MewPodVectorPtr**)(
            (uint8_t*)scene_manager + MEW_OFF_SCENE_COMPONENT_LISTS);
        if (!components ||
            !AcMewReadableRange(components, sizeof(*components)) ||
            components->size == 0U ||
            components->size > components->capacity ||
            components->size > SIZE_MAX / sizeof(*components->data) ||
            !components->data) {
            return NULL;
        }
        data_size = (size_t)components->size * sizeof(*components->data);
        if (!AcMewReadableRange(components->data, data_size)) {
            return NULL;
        }
        return components;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

void* AcMewGetValidatedComponentRoot(void* component) {
    void* root;
    void* child_collection;
    void** vtable;
    if (!component || !AcMewReadableRange(component, 0x40U)) {
        return NULL;
    }
    __try {
        root = *(void**)((uint8_t*)component + 0x38U);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
    if (!root || root == component || !AcMewReadableRange(root, 0x88U)) {
        return NULL;
    }
    __try {
        child_collection = *(void**)((uint8_t*)root + 0x80U);
        if (!AcMewReadableRange(child_collection, sizeof(void*))) {
            return NULL;
        }
        vtable = *(void***)child_collection;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
    if (!AcMewIsDisplayContainer(vtable) ||
        !AcMewReadableRange(vtable, 4U * sizeof(void*)) ||
        !AcMewExecutableAddress(vtable[0]) ||
        !AcMewExecutableAddress(vtable[3])) {
        return NULL;
    }
    return root;
}
