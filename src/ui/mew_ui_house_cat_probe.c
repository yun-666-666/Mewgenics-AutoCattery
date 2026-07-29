#include "mew_ui_house_cat_probe.h"
#include "mew_ui_scene_components.h"

#include <limits.h>
#include <stdlib.h>
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
    if (!component || !value) {
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
    void*** components_out) {
    MewPodVectorPtr* all;
    void** components;
    uint32_t index;
    size_t count;
    if (!scene_manager || !components_out) {
        return 0U;
    }
    *components_out = NULL;
    components = NULL;
    __try {
        all = AcMewGetValidatedSceneComponents(scene_manager);
        if (!all || all->size > SIZE_MAX / sizeof(*components)) {
            return 0U;
        }
        components = (void**)calloc(all->size, sizeof(*components));
        if (!components) {
            return 0U;
        }
        count = 0U;
        for (index = 0U; index < all->size; ++index) {
            MewNarrowString type_name;
            void* component = all->data[index];
            if (AcGetTypeName(component, &type_name) &&
                AcTypeEquals(&type_name, "HouseCat")) {
                components[count++] = component;
            }
        }
        if (count == 0U) {
            free(components);
            return 0U;
        }
        *components_out = components;
        return count;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        free(components);
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
    size_t* mapping,
    uint8_t* seen) {
    size_t component_index;
    size_t matched;
    memset(seen, 0, cat_count * sizeof(*seen));
    matched = 0U;
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
        if (cat_index < 0) {
            continue;
        }
        if (seen[cat_index]) {
            return 0;
        }
        seen[cat_index] = 1U;
        mapping[cat_index] = component_index;
        ++matched;
    }
    return matched == cat_count;
}
AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void* scene_manager,
    const int64_t* cat_ids,
    size_t cat_id_count,
    AcMewHouseCatMatch* matches,
    size_t match_capacity) {
    AcMewHouseCatIdentityProbe result;
    void** components;
    size_t* first_mapping;
    size_t* mapping;
    uint8_t* seen;
    size_t component_count;
    size_t offset;
    uint8_t width;
    memset(&result, 0, sizeof(result));
    components = NULL;
    first_mapping = NULL;
    mapping = NULL;
    seen = NULL;
    result.consistent_mapping = 1U;
    if (!cat_ids || !matches || cat_id_count == 0U ||
        cat_id_count > UINT32_MAX || match_capacity < cat_id_count ||
        cat_id_count > SIZE_MAX / sizeof(*first_mapping)) {
        return result;
    }
    result.requested_cat_count = (uint32_t)cat_id_count;
    component_count = AcFindHouseCats(scene_manager, &components);
    result.house_cat_count = (uint32_t)component_count;
    if (component_count < cat_id_count) {
        goto cleanup;
    }
    for (offset = 0U; offset < component_count; ++offset) {
        if (AcReadableBytes(components[offset]) < AC_SCAN_LIMIT) {
            goto cleanup;
        }
    }
    first_mapping = (size_t*)calloc(cat_id_count, sizeof(*first_mapping));
    mapping = (size_t*)calloc(cat_id_count, sizeof(*mapping));
    seen = (uint8_t*)calloc(cat_id_count, sizeof(*seen));
    if (!first_mapping || !mapping || !seen) {
        goto cleanup;
    }
    for (offset = AC_SCAN_BEGIN;
         offset + 8U <= AC_SCAN_LIMIT;
         offset += 4U) {
        for (width = 8U; width >= 4U; width -= 4U) {
            size_t index;
            memset(mapping, 0, cat_id_count * sizeof(*mapping));
            if (!AcEvaluateLayout(
                    components,
                    component_count,
                    cat_ids,
                    cat_id_count,
                    offset,
                    width,
                    mapping,
                    seen)) {
                continue;
            }
            ++result.valid_layout_count;
            if (result.valid_layout_count == 1U) {
                result.first_identity_offset = (uint32_t)offset;
                result.first_identity_width = width;
                memcpy(
                    first_mapping,
                    mapping,
                    cat_id_count * sizeof(*first_mapping));
            } else if (
                memcmp(
                    first_mapping,
                    mapping,
                    cat_id_count * sizeof(*first_mapping)) != 0) {
                result.consistent_mapping = 0U;
            }
            for (index = 0U; index < cat_id_count; ++index) {
                if (mapping[index] >= result.house_cat_count) {
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
        for (index = 0U; index < cat_id_count; ++index) {
            void* component = components[first_mapping[index]];
            matches[index].cat_id = cat_ids[index];
            matches[index].component = component;
            matches[index].root_node = AcRootNode(component);
        }
        result.match_count = cat_id_count;
    }
cleanup:
    free(seen);
    free(mapping);
    free(first_mapping);
    free(components);
    return result;
}
