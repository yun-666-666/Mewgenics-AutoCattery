#include "mew_ui_poop_adapter.h"
#include "mew_ui_scene_components.h"
#include <string.h>
#include <windows.h>

/* Quick-Cleanup's poop path was the reference; see THIRD_PARTY_NOTICES.md.
   Verified against the current executable: FurniturePiece RTTI, item key comparison,
   room-cell release, item removal, and deferred component destruction. */
enum { AC_POP_POOP_RVA = 0x2EF9D0, AC_FURNITURE_VTABLE_RVA = 0xEE6858 };

AcPoopCleanupResult AcMewCleanPoopComponents(
    void* const* components, size_t count, const void* furniture_vtable,
    AcPoopPopFn pop) {
    AcPoopCleanupResult result = {0};
    size_t index;
    if ((!components && count) || !furniture_vtable || !pop) return result;
    __try {
        for (index = 0; index < count; ++index) {
            uint8_t* component = (uint8_t*)components[index];
            uint8_t* item;
            MewNarrowString* key;
            if (!component || *(void**)component != furniture_vtable ||
                component[0xF] != 0) continue;
            item = *(uint8_t**)(component + 0x2D8);
            if (!item) continue;
            key = (MewNarrowString*)(item + 8);
            if (MewUI_GetNarrowStringSize(key) != 4 ||
                memcmp(MewUI_GetNarrowStringData(key), "poop", 4) != 0) continue;
            pop(component);
            /* The native function clears this before scheduling destruction. */
            if (*(void**)(component + 0x2D8) != NULL) return result;
            ++result.cleaned;
        }
        result.completed = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.seh_code = GetExceptionCode();
    }
    return result;
}

AcPoopCleanupResult AcMewCleanHousePoop(void* scene_manager) {
    AcPoopCleanupResult failed = {0};
    const uint8_t* image = (const uint8_t*)GetModuleHandleW(NULL);
    MewPodVectorPtr* components;
    /* Follow the existing adapters' runtime ABI gate before any native call. */
    static const uint8_t entry[] = {
        0x40,0x53,0x48,0x83,0xEC,0x50,0x48,0x8B,0xD9,
        0x48,0x8B,0x89,0xD8,0x02,0x00,0x00
    };
    __try {
        const uint32_t* locator;
        if (!image || !scene_manager ||
            *((uint8_t*)scene_manager + 0x4B0) != 0 ||
            memcmp(image + AC_POP_POOP_RVA, entry, sizeof(entry)) != 0) return failed;
        locator = *(const uint32_t**)(image + AC_FURNITURE_VTABLE_RVA - sizeof(void*));
        if (locator[0] != 1 || locator[1] != 0 ||
            (const uint8_t*)locator != image + locator[5] ||
            strcmp((const char*)(image + locator[3] + 16), ".?AVFurniturePiece@glaiel@@") != 0)
            return failed;
        components = AcMewGetValidatedSceneComponents(scene_manager);
        if (!components) return failed;
        return AcMewCleanPoopComponents(components->data, components->size,
            image + AC_FURNITURE_VTABLE_RVA, (AcPoopPopFn)(image + AC_POP_POOP_RVA));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        failed.seh_code = GetExceptionCode();
        return failed;
    }
}
