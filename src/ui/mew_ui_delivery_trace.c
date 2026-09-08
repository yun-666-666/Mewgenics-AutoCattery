#include "mew_ui_delivery_trace.h"
#include "mew_ui_scene_components.h"
#include <windows.h>
#include <string.h>

static int TypeEquals(void* component, const char* expected) {
    MewNarrowString name;
    MewComponent* typed = (MewComponent*)component;
    if (!typed || !typed->vtable || !typed->vtable->GetObjectTypeSTR) return 0;
    memset(&name, 0, sizeof(name));
    typed->vtable->GetObjectTypeSTR(component, &name);
    return MewUI_GetNarrowStringSize(&name) == strlen(expected) &&
        memcmp(MewUI_GetNarrowStringData(&name), expected, strlen(expected)) == 0;
}

AcDeliveryTrace AcMewReadDeliveryTrace(void* scene) {
    AcDeliveryTrace result;
    memset(&result, 0, sizeof(result));
    if (!scene) return result;
    __try {
        const uint8_t* image = (const uint8_t*)GetModuleHandleW(NULL);
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)image;
        const IMAGE_NT_HEADERS64* nt;
        MewPodVectorPtr* components;
        uint8_t* drawer = NULL;
        uint32_t index;
        /* Current executable's Organ Grinder completion callback. This probe
           never calls it; its instructions establish the observed layout. */
        static const uint8_t callback_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0x05,0xff,0xc3,0x15,0x01,0x48,0x8b,0xf9
        };
        if (!image || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            return result;
        nt = (const IMAGE_NT_HEADERS64*)(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.SizeOfImage <= 0x27e8e3 ||
            memcmp(image + 0x27e820, callback_start, sizeof(callback_start)) != 0)
            return result;
        result.layout_valid = 1;
        components = AcMewGetValidatedSceneComponents(scene);
        if (!components) return result;
        for (index = 0; index < components->size; ++index) {
            if (TypeEquals(components->data[index], "NPCMapDrawer")) {
                drawer = (uint8_t*)components->data[index];
                ++result.drawer_count;
            } else if (TypeEquals(components->data[index], "HouseCat")) {
                ++result.house_cat_count;
            }
        }
        if (result.drawer_count != 1) return result;
        result.open = drawer[0x40];
        result.cat_mode = drawer[0x41];
        memcpy(&result.npc_result, drawer + 0xb8, sizeof(result.npc_result));
        {
            uint8_t* cat = *(uint8_t**)(drawer + 0x48);
            if (cat && *(uint64_t*)(cat - 8) == *(uint64_t*)(drawer + 0x50) &&
                MewUI_IsComponentInScene(scene, cat) && TypeEquals(cat, "HouseCat")) {
                result.selected_valid = 1;
                memcpy(&result.cat_id, cat + 0x80, sizeof(result.cat_id));
            }
        }
        {
            uint8_t* callback = *(uint8_t**)(drawer + 0xb0);
            if (callback) {
                uintptr_t address = *(uintptr_t*)(*(uint8_t**)callback + 0x10);
                const uintptr_t base = (uintptr_t)image;
                if (address >= base && address - base < nt->OptionalHeader.SizeOfImage)
                    result.callback_rva = address - base;
                result.callback_bound_to_drawer = *(void**)(callback + 8) == drawer;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.seh_code = GetExceptionCode();
    }
    return result;
}
