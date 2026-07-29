#include "mew_ui_house_detail_adapter.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

#define AC_RVA_HOUSE_OPEN_CAT_DETAILS 0x00EBEF0ULL
#define AC_RVA_HOUSE_CAT_DETAIL_TARGET 0x00EFCB0ULL
#define AC_RVA_HOUSE_CAT_RESOLVE_CALL_SITE 0x001FE082ULL
#define AC_RVA_HOUSE_CLICK_CALL_SITE 0x001FE2D5ULL

typedef void*(__fastcall* AcResolveHouseCatDetailTargetFn)(
    void* house_cat);
typedef void(__fastcall* AcOpenCatDetailsFn)(
    void* house_drawer,
    void* cat_detail_target,
    uint8_t show_drawer);

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

static int AcTypeEquals(void* component, const char* expected) {
    MewNarrowString type_name;
    const char* data;
    size_t size;
    const size_t expected_size = strlen(expected);
    if (!AcGetTypeName(component, &type_name)) {
        return 0;
    }
    data = MewUI_GetNarrowStringData(&type_name);
    size = MewUI_GetNarrowStringSize(&type_name);
    return data && size == expected_size &&
           memcmp(data, expected, expected_size) == 0;
}

static int AcSignaturesMatch(uint8_t* module_base) {
    static const uint8_t function_signature[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0xD9,
        0x48, 0x85, 0xD2
    };
    static const uint8_t resolver_signature[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24,
        0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x71,
        0x18
    };
    static const uint8_t resolver_call_site_signature[] = {
        0x49, 0x8B, 0xCF, 0xE8, 0x26, 0x1C, 0xEF, 0xFF,
        0x4C, 0x8B, 0xF8
    };
    static const uint8_t call_site_signature[] = {
        0x41, 0xB0, 0x01, 0x49, 0x8B, 0xD7, 0x48, 0x8B,
        0xCF, 0xE8, 0x0D, 0xDC, 0xEE, 0xFF
    };
    __try {
        return memcmp(
                   module_base + AC_RVA_HOUSE_OPEN_CAT_DETAILS,
                   function_signature,
                   sizeof(function_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_CAT_DETAIL_TARGET,
                   resolver_signature,
                   sizeof(resolver_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_CAT_RESOLVE_CALL_SITE,
                   resolver_call_site_signature,
                   sizeof(resolver_call_site_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_CLICK_CALL_SITE,
                   call_site_signature,
                   sizeof(call_site_signature)) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void* AcFindUniqueHouseDrawer(void* scene_manager) {
    MewPodVectorPtr* components;
    void* drawer;
    uint32_t index;
    drawer = NULL;
    __try {
        components = *(MewPodVectorPtr**)((uint8_t*)scene_manager +
                                          MEW_OFF_SCENE_COMPONENT_LISTS);
        if (!components || !components->data ||
            components->size > 4096U) {
            return NULL;
        }
        for (index = 0U; index < components->size; ++index) {
            void* component = components->data[index];
            if (!AcTypeEquals(component, "HouseDrawerUI")) {
                continue;
            }
            if (drawer) {
                return NULL;
            }
            drawer = component;
        }
        return drawer;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

AcMewHouseDetailResult AcMewOpenHouseCatDetails(
    void* scene_manager,
    void* house_cat_component) {
    AcMewHouseDetailResult result;
    uint8_t* module_base;
    void* drawer;
    void* detail_target;
    MewNarrowString detail_target_type;
    memset(&result, 0, sizeof(result));
    if (!scene_manager ||
        MewUI_IsSceneReadyForUITick(scene_manager) == 0 ||
        MewUI_IsSceneDestroying(scene_manager) != 0) {
        return result;
    }
    result.scene_valid = 1U;
    module_base = (uint8_t*)GetModuleHandleW(NULL);
    if (!module_base || !AcSignaturesMatch(module_base)) {
        return result;
    }
    result.signature_valid = 1U;
    drawer = AcFindUniqueHouseDrawer(scene_manager);
    if (!drawer) {
        return result;
    }
    result.drawer_unique = 1U;
    if (!house_cat_component ||
        MewUI_IsComponentInScene(
            scene_manager,
            house_cat_component) == 0 ||
        !AcTypeEquals(house_cat_component, "HouseCat")) {
        return result;
    }
    result.cat_valid = 1U;
    __try {
        AcResolveHouseCatDetailTargetFn resolve_target =
            (AcResolveHouseCatDetailTargetFn)(
                module_base + AC_RVA_HOUSE_CAT_DETAIL_TARGET);
        detail_target = resolve_target(house_cat_component);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return result;
    }
    if (!detail_target ||
        !AcGetTypeName(detail_target, &detail_target_type)) {
        return result;
    }
    result.detail_target_valid = 1U;
    __try {
        AcOpenCatDetailsFn open_details =
            (AcOpenCatDetailsFn)(
                module_base + AC_RVA_HOUSE_OPEN_CAT_DETAILS);
        open_details(drawer, detail_target, 1U);
        result.invoked = 1U;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result.invoked = 0U;
    }
    return result;
}
