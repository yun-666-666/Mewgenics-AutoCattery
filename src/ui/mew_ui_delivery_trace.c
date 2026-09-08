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

#include "mew_ui_house_detail_adapter.h"

static void* UniqueComponent(void* scene, const char* type) {
    MewPodVectorPtr* components = AcMewGetValidatedSceneComponents(scene);
    void* found = NULL;
    uint32_t i;
    if (!components) return NULL;
    for (i = 0; i < components->size; ++i) {
        if (!TypeEquals(components->data[i], type)) continue;
        if (found) return NULL;
        found = components->data[i];
    }
    return found;
}

static int DeadCatValid(void* scene, uint8_t* cat, int64_t id) {
    uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
    uint8_t* game;
    uint8_t* data;
    typedef void* (__fastcall *ResolveCat)(void*, int64_t);
    if (!cat || !MewUI_IsComponentInScene(scene, cat) || !TypeEquals(cat, "HouseCat") ||
        *(int64_t*)(cat + 0x80) != id) return 0;
    /* Globals and CatData resolver are read by the verified 0x27E820 callback. */
    game = *(uint8_t**)(image + 0x13dac30);
    if (!game) return 0;
    data = (uint8_t*)((ResolveCat)(image + 0xd7220))(*(void**)(game + 0x598), id);
    return data && *(int64_t*)(data + 0xc40) >= 0;
}

int AcMewCloseDeliveryDetails(void* scene) {
    __try {
        uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
        AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* stats;
        typedef void (__fastcall *CloseDrawer)(void*);
        /* Current native close path resolves the drawer manager, closes only
           when this drawer is active, and releases the active drawer slot. */
        static const uint8_t close_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0xf9,0x48,0x8b,0x49,0x18
        };
        if (!state.layout_valid || state.seh_code || state.callback_rva ||
            state.selected_valid || memcmp(image + 0x2048d0, close_start, sizeof(close_start))) return 0;
        stats = (uint8_t*)UniqueComponent(scene, "CatStatsDrawer");
        if (!stats || !*(void**)(stats + 0x38)) return 0;
        ((CloseDrawer)(image + 0x2048d0))(*(void**)(stats + 0x38));
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewOpenDeadCatPipe(void* scene, void* cat, int64_t id) {
    __try {
        uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
        AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* drawer;
        void* closure[2];
        AcMewHouseDetailResult opened;
        typedef void (__fastcall *PipeClick)(void*);
        static const uint8_t pipe_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0x41,0x08,0x48,0x8b,0xf9,0x48,0x8b,0x58,0x38
        };
        if (!state.layout_valid || state.drawer_count != 1 || state.seh_code ||
            state.selected_valid || state.callback_rva ||
            memcmp(image + 0xedf00, pipe_start, sizeof(pipe_start)) ||
            !DeadCatValid(scene, (uint8_t*)cat, id)) return 0;
        opened = AcMewOpenHouseCatDetails(scene, cat);
        if (!opened.invoked) return 0;
        drawer = (uint8_t*)UniqueComponent(scene, "CatStatsDrawer");
        if (!drawer || *(void**)(drawer + 0x78) != cat ||
            *(uint64_t*)(drawer + 0x80) != *(uint64_t*)((uint8_t*)cat - 8)) return 0;
        /* Native closure reads only its captured CatStatsDrawer at +8. */
        closure[0] = NULL;
        closure[1] = drawer;
        ((PipeClick)(image + 0xedf00))(closure);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewChooseDeadCatRecipient(void* scene, int64_t id) {
    __try {
        uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
        AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* drawer;
        typedef void (__fastcall *ChooseRecipient)(void*);
        static const uint8_t choose_start[] = {
            0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,
            0x55,0x48,0x8d,0x6c,0x24,0xa9,0x48,0x81,0xec,0xb0,0,0,0
        };
        if (!state.layout_valid || state.drawer_count != 1 || state.seh_code ||
            !state.cat_mode || !state.selected_valid || state.cat_id != id ||
            state.callback_rva ||
            memcmp(image + 0x27c500, choose_start, sizeof(choose_start))) return 0;
        drawer = (uint8_t*)UniqueComponent(scene, "NPCMapDrawer");
        if (!drawer || !DeadCatValid(scene, *(uint8_t**)(drawer + 0x48), id)) return 0;
        ((ChooseRecipient)(image + 0x27c500))(drawer);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
