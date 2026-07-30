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
#define AC_VERIFIED_IDENTITY_OFFSET 0x80U
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
static int AcReadIdentity(
    void* component,
    size_t offset,
    uint8_t width,
    int64_t* value) {
    const size_t byte_count = width == 8U ? 8U : width == 4U ? 4U : 0U;
    const uintptr_t start = (uintptr_t)component;
    const void* source;
    if (!component || !value) {
        return 0;
    }
    if (byte_count == 0U || offset > UINTPTR_MAX - start) {
        return 0;
    }
    source = (const void*)(start + offset);
    if (!AcReadableRange(source, byte_count)) {
        return 0;
    }
    __try {
        if (width == 8U) {
            memcpy(value, source, 8U);
            return 1;
        }
        if (width == 4U) {
            uint32_t narrow;
            memcpy(&narrow, source, 4U);
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

size_t AcMewCountHouseCats(void* scene_manager) {
    void** components;
    const size_t count =
        AcFindHouseCats(scene_manager, &components);
    free(components);
    return count;
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
    first_mapping = (size_t*)calloc(cat_id_count, sizeof(*first_mapping));
    mapping = (size_t*)calloc(cat_id_count, sizeof(*mapping));
    seen = (uint8_t*)calloc(cat_id_count, sizeof(*seen));
    if (!first_mapping || !mapping || !seen) {
        goto cleanup;
    }
    /*
     * The current supported executable was verified independently to store
     * the persisted CatId at HouseCat+0x80. Prefer that exact layout before
     * the diagnostic scan: large saves can contain the same integers in
     * unrelated fields, which made the all-layout consistency check reject
     * an otherwise exact mapping.
     */
    if (AcEvaluateLayout(
            components,
            component_count,
            cat_ids,
            cat_id_count,
            AC_VERIFIED_IDENTITY_OFFSET,
            8U,
            mapping,
            seen)) {
        result.valid_layout_count = 1U;
        result.first_identity_offset =
            AC_VERIFIED_IDENTITY_OFFSET;
        result.first_identity_width = 8U;
        memcpy(
            first_mapping,
            mapping,
            cat_id_count * sizeof(*first_mapping));
        goto layout_complete;
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

layout_complete:
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
