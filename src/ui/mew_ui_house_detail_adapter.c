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
#define AC_RVA_HOUSE_DRAWER_FROM_CLICK_MANAGER 0x01A93F0ULL
#define AC_RVA_HOUSE_CAT_RESOLVE_CALL_SITE 0x001FE082ULL
#define AC_RVA_HOUSE_DRAWER_RESOLVE_CALL_SITE 0x001FE262ULL
#define AC_RVA_HOUSE_CLICK_CALL_SITE 0x001FE2D5ULL

typedef void*(__fastcall* AcResolveHouseCatDetailTargetFn)(
    void* house_cat);
typedef void*(__fastcall* AcResolveHouseDrawerFn)(
    void* house_cat_click_manager);
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
    static const uint8_t drawer_resolver_signature[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x48, 0x8B, 0x79, 0x18, 0xBA, 0xD5,
        0x01, 0x00, 0x00, 0x48, 0x8B, 0x5F, 0x08, 0x48
    };
    static const uint8_t resolver_call_site_signature[] = {
        0x49, 0x8B, 0xCF, 0xE8, 0x26, 0x1C, 0xEF, 0xFF,
        0x4C, 0x8B, 0xF8
    };
    static const uint8_t drawer_resolver_call_site_signature[] = {
        0x49, 0x8B, 0xCE, 0xE8, 0x86, 0xB1, 0xFA, 0xFF,
        0x48, 0x8B, 0xF8, 0x49, 0x83, 0x7E, 0x38, 0x00
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
                   module_base + AC_RVA_HOUSE_DRAWER_FROM_CLICK_MANAGER,
                   drawer_resolver_signature,
                   sizeof(drawer_resolver_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_CAT_RESOLVE_CALL_SITE,
                   resolver_call_site_signature,
                   sizeof(resolver_call_site_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_DRAWER_RESOLVE_CALL_SITE,
                   drawer_resolver_call_site_signature,
                   sizeof(drawer_resolver_call_site_signature)) == 0 &&
               memcmp(
                   module_base + AC_RVA_HOUSE_CLICK_CALL_SITE,
                   call_site_signature,
                   sizeof(call_site_signature)) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void* AcFindUniqueComponent(
    void* scene_manager,
    const char* expected_type) {
    MewPodVectorPtr* components;
    void* match;
    uint32_t index;
    match = NULL;
    __try {
        components = *(MewPodVectorPtr**)((uint8_t*)scene_manager +
                                          MEW_OFF_SCENE_COMPONENT_LISTS);
        if (!components || !components->data ||
            components->size > 4096U) {
            return NULL;
        }
        for (index = 0U; index < components->size; ++index) {
            void* component = components->data[index];
            if (!AcTypeEquals(component, expected_type)) {
                continue;
            }
            if (match) {
                return NULL;
            }
            match = component;
        }
        return match;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

static LONG AcCaptureException(
    EXCEPTION_POINTERS* exception,
    uint8_t* module_base,
    AcMewHouseDetailResult* result,
    uint8_t failure_stage) {
    const uint8_t* exception_address;
    IMAGE_DOS_HEADER* dos_header;
    IMAGE_NT_HEADERS* nt_headers;
    size_t image_size;
    result->failure_stage = failure_stage;
    if (!exception || !exception->ExceptionRecord) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    result->seh_code = exception->ExceptionRecord->ExceptionCode;
    exception_address =
        (const uint8_t*)exception->ExceptionRecord->ExceptionAddress;
    __try {
        dos_header = (IMAGE_DOS_HEADER*)module_base;
        nt_headers = (IMAGE_NT_HEADERS*)(
            module_base + dos_header->e_lfanew);
        image_size = nt_headers->OptionalHeader.SizeOfImage;
        if (exception_address >= module_base &&
            exception_address < module_base + image_size) {
            result->exception_rva =
                (unsigned long long)(exception_address - module_base);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result->exception_rva = 0ULL;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

AcMewHouseDetailResult AcMewOpenHouseCatDetails(
    void* scene_manager,
    void* house_cat_component) {
    AcMewHouseDetailResult result;
    uint8_t* module_base;
    void* click_manager;
    void* drawer;
    void* scene_drawer;
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
    click_manager = AcFindUniqueComponent(
        scene_manager,
        "HouseCatClickManager");
    if (!click_manager) {
        return result;
    }
    result.click_manager_valid = 1U;
    __try {
        AcResolveHouseDrawerFn resolve_drawer =
            (AcResolveHouseDrawerFn)(
                module_base +
                AC_RVA_HOUSE_DRAWER_FROM_CLICK_MANAGER);
        drawer = resolve_drawer(click_manager);
    }
    __except (AcCaptureException(
        GetExceptionInformation(),
        module_base,
        &result,
        1U)) {
        return result;
    }
    if (!drawer || !AcTypeEquals(drawer, "HouseDrawerUI")) {
        return result;
    }
    result.drawer_unique = 1U;
    scene_drawer = AcFindUniqueComponent(
        scene_manager,
        "HouseDrawerUI");
    if (scene_drawer) {
        result.scene_drawer_unique = 1U;
        result.drawer_matches_scene =
            scene_drawer == drawer ? 1U : 0U;
    }
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
    __except (AcCaptureException(
        GetExceptionInformation(),
        module_base,
        &result,
        2U)) {
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
    __except (AcCaptureException(
        GetExceptionInformation(),
        module_base,
        &result,
        3U)) {
        result.invoked = 0U;
    }
    return result;
}
