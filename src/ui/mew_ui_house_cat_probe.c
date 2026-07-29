#include "mew_ui_house_cat_probe.h"

#include <limits.h>
#include <string.h>
#include <windows.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

#define AC_SCAN_BEGIN 0x40U
#define AC_SCAN_LIMIT 0x800U
static int AcGetTypeName(void* component, MewNarrowString* output) {
    MewComponent* typed;
    if (!component || !output) {
        return 0;
    }
    typed = (MewComponent*)component;
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        memset(output, 0, sizeof(*output));
        typed->vtable->GetObjectTypeSTR(component, output);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static int AcTypeEquals(
    const MewNarrowString* type_name,
    const char* literal) {
    const char* data;
    size_t size;
    const size_t literal_size = strlen(literal);
    data = MewUI_GetNarrowStringData(type_name);
    size = MewUI_GetNarrowStringSize(type_name);
    return data && size == literal_size &&
           memcmp(data, literal, literal_size) == 0;
}
static size_t AcReadableBytes(void* pointer) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t start;
    uintptr_t end;
    DWORD blocked;
    if (!pointer ||
        VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT) {
        return 0U;
    }
    blocked = PAGE_GUARD | PAGE_NOACCESS;
    if ((info.Protect & blocked) != 0U) {
        return 0U;
    }
    start = (uintptr_t)pointer;
    end = (uintptr_t)info.BaseAddress + (uintptr_t)info.RegionSize;
    if (end <= start) {
        return 0U;
    }
    return (size_t)(end - start);
}
static int AcReadIdentity(
    void* component,
    size_t offset,
    uint8_t width,
    int64_t* value) {
    const size_t readable = AcReadableBytes(component);
    if (!value || offset > readable || width > readable - offset) {
        return 0;
    }
    __try {
        if (width == 8U) {
            memcpy(value, (uint8_t*)component + offset, 8U);
            return 1;
        }
        if (width == 4U) {
            uint32_t narrow;
            memcpy(&narrow, (uint8_t*)component + offset, 4U);
            *value = narrow;
            return 1;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}
static void* AcRootNode(void* component) {
    void* root;
    __try {
        root = *(void**)((uint8_t*)component + 0x38U);
        return root != component ? root : NULL;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}
static size_t AcFindHouseCats(
    void* scene_manager,
    void** components,
    size_t capacity) {
    MewPodVectorPtr* all;
    uint32_t index;
    size_t count;
    if (!scene_manager || !components || capacity == 0U) {
        return 0U;
    }
    __try {
        all = *(MewPodVectorPtr**)((uint8_t*)scene_manager +
                                   MEW_OFF_SCENE_COMPONENT_LISTS);
        if (!all || !all->data || all->size > 4096U) {
            return 0U;
        }
        count = 0U;
        for (index = 0U; index < all->size && count < capacity; ++index) {
            MewNarrowString type_name;
            void* component = all->data[index];
            if (AcGetTypeName(component, &type_name) &&
                AcTypeEquals(&type_name, "HouseCat")) {
                components[count++] = component;
            }
        }
        return count;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
}
static int AcCatIndex(
    const int64_t* cat_ids,
    size_t cat_count,
    int64_t value) {
    size_t index;
    for (index = 0U; index < cat_count; ++index) {
        if (cat_ids[index] == value) {
            return (int)index;
        }
    }
    return -1;
}
static int AcEvaluateLayout(
    void** components,
    size_t component_count,
    const int64_t* cat_ids,
    size_t cat_count,
    size_t offset,
    uint8_t width,
    uint8_t* mapping) {
    uint8_t seen[AC_MEW_HOUSE_CAT_MATCH_CAPACITY];
    size_t component_index;
    memset(seen, 0, sizeof(seen));
    for (component_index = 0U;
         component_index < component_count;
         ++component_index) {
        int64_t value;
        int cat_index;
        if (!AcReadIdentity(
                components[component_index],
                offset,
                width,
                &value)) {
            return 0;
        }
        cat_index = AcCatIndex(cat_ids, cat_count, value);
        if (cat_index < 0 || seen[cat_index]) {
            return 0;
        }
        seen[cat_index] = 1U;
        mapping[cat_index] = (uint8_t)component_index;
    }
    return 1;
}
AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void* scene_manager,
    const int64_t* cat_ids,
    size_t cat_id_count) {
    AcMewHouseCatIdentityProbe result;
    void* components[AC_MEW_HOUSE_CAT_MATCH_CAPACITY];
    uint8_t first_mapping[AC_MEW_HOUSE_CAT_MATCH_CAPACITY];
    size_t offset;
    uint8_t width;
    memset(&result, 0, sizeof(result));
    memset(components, 0, sizeof(components));
    memset(first_mapping, 0, sizeof(first_mapping));
    result.requested_cat_count = (uint32_t)cat_id_count;
    result.consistent_mapping = 1U;
    if (!cat_ids || cat_id_count == 0U ||
        cat_id_count > AC_MEW_HOUSE_CAT_MATCH_CAPACITY) {
        return result;
    }
    result.house_cat_count = (uint32_t)AcFindHouseCats(
        scene_manager,
        components,
        AC_MEW_HOUSE_CAT_MATCH_CAPACITY);
    if (result.house_cat_count != cat_id_count) {
        return result;
    }

    for (offset = AC_SCAN_BEGIN;
         offset + 8U <= AC_SCAN_LIMIT;
         offset += 4U) {
        for (width = 8U; width >= 4U; width -= 4U) {
            uint8_t mapping[AC_MEW_HOUSE_CAT_MATCH_CAPACITY];
            size_t index;
            memset(mapping, 0, sizeof(mapping));
            if (!AcEvaluateLayout(
                    components,
                    cat_id_count,
                    cat_ids,
                    cat_id_count,
                    offset,
                    width,
                    mapping)) {
                continue;
            }
            ++result.valid_layout_count;
            if (result.valid_layout_count == 1U) {
                result.first_identity_offset = (uint32_t)offset;
                result.first_identity_width = width;
                memcpy(first_mapping, mapping, cat_id_count);
            } else if (
                memcmp(first_mapping, mapping, cat_id_count) != 0) {
                result.consistent_mapping = 0U;
            }
            for (index = 0U; index < cat_id_count; ++index) {
                if (mapping[index] >= cat_id_count) {
                    result.consistent_mapping = 0U;
                }
            }
        }
    }

    result.stable_bijection =
        result.valid_layout_count > 0U &&
        result.consistent_mapping;
    if (result.stable_bijection) {
        size_t index;
        result.match_count = cat_id_count;
        for (index = 0U; index < cat_id_count; ++index) {
            void* component = components[first_mapping[index]];
            result.matches[index].cat_id = cat_ids[index];
            result.matches[index].component = component;
            result.matches[index].root_node = AcRootNode(component);
        }
    }
    return result;
}
