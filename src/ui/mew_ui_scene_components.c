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
    return root;
}

int AcMewComponentTypeEquals(void* component, const char* expected_type) {
    MewComponent* typed;
    MewNarrowString type_name;
    const char* data;
    size_t size;
    size_t expected_size;
    if (!component || !expected_type || expected_type[0] == '\0') {
        return 0;
    }
    typed = (MewComponent*)component;
    memset(&type_name, 0, sizeof(type_name));
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        typed->vtable->GetObjectTypeSTR(component, &type_name);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    data = MewUI_GetNarrowStringData(&type_name);
    size = MewUI_GetNarrowStringSize(&type_name);
    expected_size = strlen(expected_type);
    return data && size == expected_size &&
           memcmp(data, expected_type, expected_size) == 0;
}
