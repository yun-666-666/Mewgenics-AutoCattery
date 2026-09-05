#include "mew_ui_house_detail_adapter.h"
#include "mew_ui_scene_components.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

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

typedef struct AcMewHouseDetailCandidate {
    AcMewHouseDetailLayout layout;
    uintptr_t cat_resolve_call_site;
    uintptr_t drawer_resolve_call_site;
    uintptr_t click_call_site;
} AcMewHouseDetailCandidate;

static int AcBytesMatch(
    const uint8_t* image,
    size_t image_size,
    uintptr_t rva,
    const uint8_t* signature,
    size_t signature_size) {
    return image && signature && rva <= image_size &&
        image_size - rva >= signature_size &&
        memcmp(image + rva, signature, signature_size) == 0;
}

static int AcRelativeCallTargets(
    const uint8_t* image,
    size_t image_size,
    uintptr_t call_rva,
    uintptr_t target_rva) {
    int32_t displacement;
    uintptr_t resolved;
    if (!image || call_rva > image_size ||
        image_size - call_rva < 5U || image[call_rva] != 0xE8U) {
        return 0;
    }
    memcpy(&displacement, image + call_rva + 1U, sizeof(displacement));
    resolved = (uintptr_t)(
        (intptr_t)(call_rva + 5U) + (intptr_t)displacement);
    return resolved == target_rva;
}

static int AcCandidateMatches(
    const uint8_t* image,
    size_t image_size,
    const AcMewHouseDetailCandidate* candidate) {
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
    static const uint8_t resolver_call_site_prefix[] = {
        0x49, 0x8B, 0xCF, 0xE8
    };
    static const uint8_t resolver_call_site_suffix[] = {
        0x4C, 0x8B, 0xF8
    };
    static const uint8_t drawer_resolver_call_site_prefix[] = {
        0x49, 0x8B, 0xCE, 0xE8
    };
    static const uint8_t drawer_resolver_call_site_suffix[] = {
        0x48, 0x8B, 0xF8, 0x49, 0x83, 0x7E, 0x38, 0x00
    };
    static const uint8_t call_site_prefix[] = {
        0x41, 0xB0, 0x01, 0x49, 0x8B, 0xD7, 0x48, 0x8B, 0xCF, 0xE8
    };
    return candidate &&
        AcBytesMatch(image, image_size,
            candidate->layout.open_cat_details_rva,
            function_signature, sizeof(function_signature)) &&
        AcBytesMatch(image, image_size,
            candidate->layout.cat_detail_target_rva,
            resolver_signature, sizeof(resolver_signature)) &&
        AcBytesMatch(image, image_size,
            candidate->layout.drawer_from_click_manager_rva,
            drawer_resolver_signature, sizeof(drawer_resolver_signature)) &&
        AcBytesMatch(image, image_size,
            candidate->cat_resolve_call_site,
            resolver_call_site_prefix,
            sizeof(resolver_call_site_prefix)) &&
        AcBytesMatch(image, image_size,
            candidate->cat_resolve_call_site + 8U,
            resolver_call_site_suffix,
            sizeof(resolver_call_site_suffix)) &&
        AcRelativeCallTargets(image, image_size,
            candidate->cat_resolve_call_site + 3U,
            candidate->layout.cat_detail_target_rva) &&
        AcBytesMatch(image, image_size,
            candidate->drawer_resolve_call_site,
            drawer_resolver_call_site_prefix,
            sizeof(drawer_resolver_call_site_prefix)) &&
        AcBytesMatch(image, image_size,
            candidate->drawer_resolve_call_site + 8U,
            drawer_resolver_call_site_suffix,
            sizeof(drawer_resolver_call_site_suffix)) &&
        AcRelativeCallTargets(image, image_size,
            candidate->drawer_resolve_call_site + 3U,
            candidate->layout.drawer_from_click_manager_rva) &&
        AcBytesMatch(image, image_size,
            candidate->click_call_site,
            call_site_prefix,
            sizeof(call_site_prefix)) &&
        AcRelativeCallTargets(image, image_size,
            candidate->click_call_site + 9U,
            candidate->layout.open_cat_details_rva);
}

int AcMewSelectHouseDetailLayout(
    const uint8_t* image,
    size_t image_size,
    AcMewHouseDetailLayout* output) {
    static const AcMewHouseDetailCandidate candidates[] = {
        {{0x00EBEF0U, 0x00EFCB0U, 0x01A93F0U},
         0x001FE082U, 0x001FE262U, 0x001FE2D5U},
        {{0x00EC7B0U, 0x00F0570U, 0x01A9E10U},
         0x001FEAF2U, 0x001FECD2U, 0x001FED45U}
    };
    const AcMewHouseDetailCandidate* selected = NULL;
    size_t index;
    if (!output) {
        return 0;
    }
    memset(output, 0, sizeof(*output));
    for (index = 0U; index < sizeof(candidates) / sizeof(candidates[0]);
         ++index) {
        if (!AcCandidateMatches(image, image_size, &candidates[index])) {
            continue;
        }
        if (selected) {
            return 0;
        }
        selected = &candidates[index];
    }
    if (!selected) {
        return 0;
    }
    *output = selected->layout;
    return 1;
}

static size_t AcExecutableImageSize(uint8_t* module_base) {
    __try {
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)module_base;
        IMAGE_NT_HEADERS64* nt;
        if (!module_base || dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return 0U;
        }
        nt = (IMAGE_NT_HEADERS64*)(module_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return 0U;
        }
        return nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
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
        components = AcMewGetValidatedSceneComponents(scene_manager);
        if (!components) {
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

static int AcHouseDrawerLayoutValid(void* drawer) {
    __try {
        return drawer &&
               *(void**)((uint8_t*)drawer + 0x38U) &&
               *(void**)((uint8_t*)drawer + 0x60U);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
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
    AcMewHouseDetailLayout layout;
    memset(&result, 0, sizeof(result));
    if (!scene_manager ||
        MewUI_IsSceneReadyForUITick(scene_manager) == 0 ||
        MewUI_IsSceneDestroying(scene_manager) != 0) {
        return result;
    }
    result.scene_valid = 1U;
    module_base = (uint8_t*)GetModuleHandleW(NULL);
    if (!module_base ||
        !AcMewSelectHouseDetailLayout(
            module_base,
            AcExecutableImageSize(module_base),
            &layout)) {
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
    scene_drawer = AcFindUniqueComponent(
        scene_manager,
        "HouseDrawerUI");
    if (scene_drawer) {
        result.scene_drawer_unique = 1U;
    }
    __try {
        AcResolveHouseDrawerFn resolve_drawer =
            (AcResolveHouseDrawerFn)(
                module_base +
                layout.drawer_from_click_manager_rva);
        drawer = resolve_drawer(click_manager);
    }
    __except (AcCaptureException(
        GetExceptionInformation(),
        module_base,
        &result,
        1U)) {
        return result;
    }
    if (!AcHouseDrawerLayoutValid(drawer)) {
        return result;
    }
    result.drawer_unique = 1U;
    result.drawer_matches_scene =
        scene_drawer == drawer ? 1U : 0U;
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
                module_base + layout.cat_detail_target_rva);
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
                module_base + layout.open_cat_details_rva);
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
