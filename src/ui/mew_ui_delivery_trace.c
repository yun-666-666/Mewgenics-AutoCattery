#include "mew_ui_delivery_trace.h"
#include "mew_ui_scene_components.h"
#include "mew_ui_population_recipient_policy.h"
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
        /* Native DonateCat sets +0xc0 to 0.25. NPCMapDrawer's update
           consumes it before closing its drawer and returning to details. */
        memcpy(&result.completion_delay, drawer + 0xc0, sizeof(result.completion_delay));
        {
            typedef uint8_t (__fastcall *DrawerActive)(void*);
            result.npc_drawer_active = ((DrawerActive)(image + 0x204800))(
                *(void**)(drawer + 0x38));
        }
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

static int CatValid(void* scene, uint8_t* cat, int64_t id, int require_dead) {
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
    return data && (!require_dead || *(int64_t*)(data + 0xc40) >= 0);
}

static int OpenCatPipe(void* scene, void* cat, int64_t id, int require_dead) {
    __try {
        uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
        AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* drawer;
        void* closure[2];
        AcMewHouseDetailResult opened;
        typedef void (__fastcall *PipeClick)(void*);
        typedef uint8_t (__fastcall *DrawerActive)(void*);
        static const uint8_t pipe_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0x41,0x08,0x48,0x8b,0xf9,0x48,0x8b,0x58,0x38
        };
        if (!state.layout_valid || state.drawer_count != 1 || state.seh_code ||
            state.selected_valid || state.callback_rva ||
            memcmp(image + 0xedf00, pipe_start, sizeof(pipe_start)) ||
            !CatValid(scene, (uint8_t*)cat, id, require_dead)) return 0;
        if (state.npc_drawer_active || !(state.completion_delay <= 0.0)) return 2;
        opened = AcMewOpenHouseCatDetails(scene, cat);
        if (!opened.invoked) return 0;
        drawer = (uint8_t*)UniqueComponent(scene, "CatStatsDrawer");
        if (!drawer || *(void**)(drawer + 0x78) != cat ||
            *(uint64_t*)(drawer + 0x80) != *(uint64_t*)((uint8_t*)cat - 8)) return 0;
        /* The native click silently returns if details did not acquire the
           active drawer slot. Do not report that no-op as a pipe request. */
        if (!((DrawerActive)(image + 0x204800))(*(void**)(drawer + 0x38))) return 2;
        /* Native closure reads only its captured CatStatsDrawer at +8. */
        closure[0] = NULL;
        closure[1] = drawer;
        ((PipeClick)(image + 0xedf00))(closure);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewOpenDeadCatPipe(void* scene, void* cat, int64_t id) {
    return OpenCatPipe(scene, cat, id, 1);
}

int AcMewOpenPopulationCatPipe(void* scene, void* cat, int64_t id) {
    return OpenCatPipe(scene, cat, id, 0);
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
        if (!drawer || !CatValid(scene, *(uint8_t**)(drawer + 0x48), id, 1)) return 0;
        ((ChooseRecipient)(image + 0x27c500))(drawer);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewChooseTrashRecipient(void* scene, int64_t id) {
    __try {
        uint8_t* image = (uint8_t*)GetModuleHandleW(NULL);
        AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* drawer;
        void* closure[2];
        typedef void (__fastcall *TrashClick)(void*);
        /* NPCMapDrawer's actual trash node binds closure vtable F03618.
           Invoke 27E1A0 installs completion 27E640 (F032C8), which sends
           recipient 7 through 1C1650 and removes the HouseCat via 1FF2E0. */
        static const uint8_t click_start[] = {
            0x48,0x89,0x5c,0x24,0x10,0x55,0x48,0x8d,0x6c,0x24,0xa9,
            0x48,0x81,0xec,0xb0,0,0,0,0x48,0x8b,0x59,0x08
        };
        static const uint8_t complete_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0x05,0xdf,0xc5,0x15,0x01,0x48,0x8b,0xf9
        };
        if (!state.layout_valid || state.drawer_count != 1 || state.seh_code ||
            !state.cat_mode || !state.selected_valid || state.cat_id != id ||
            state.callback_rva ||
            memcmp(image + 0x27e1a0, click_start, sizeof(click_start)) ||
            memcmp(image + 0x27e640, complete_start, sizeof(complete_start))) return 0;
        drawer = (uint8_t*)UniqueComponent(scene, "NPCMapDrawer");
        if (!drawer || !CatValid(scene, *(uint8_t**)(drawer + 0x48), id, 0)) return 0;
        closure[0] = NULL;
        closure[1] = drawer;
        ((TrashClick)(image + 0x27e1a0))(closure);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewFindPopulationRecipient(void* scene, int64_t id) {
    __try {
        const uint8_t* image = (const uint8_t*)GetModuleHandleW(NULL);
        const AcDeliveryTrace state = AcMewReadDeliveryTrace(scene);
        uint8_t* drawer;
        uint8_t* game;
        void* data;
        uint8_t* progress;
        int recipient;
        uint32_t accepts = 0U;
        typedef void* (__fastcall *ResolveCat)(void*, int64_t);
        typedef uint8_t (__fastcall *AcceptsCat)(void*, int, void*);
        static const uint8_t accepts_start[] = {
            0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,
            0x48,0x89,0x7c,0x24,0x18,0x41,0x56,0x48,0x83,0xec,0x50,
            0x49,0x8b,0xf8,0x4c,0x63,0xc2,0x48,0x8b,0xf1
        };
        if (!state.layout_valid || state.drawer_count != 1 || state.seh_code ||
            !state.cat_mode || !state.selected_valid || state.cat_id != id ||
            state.callback_rva || memcmp(image + 0x276600,
                accepts_start, sizeof(accepts_start))) return -1;
        drawer = (uint8_t*)UniqueComponent(scene, "NPCMapDrawer");
        if (!drawer || !CatValid(scene, *(uint8_t**)(drawer + 0x48), id, 0)) return -1;
        game = *(uint8_t**)(image + 0x13dac30);
        progress = *(uint8_t**)(game + 0x5a8);
        data = ((ResolveCat)(image + 0xd7220))(*(void**)(game + 0x598), id);
        if (!progress || !data) return -1;
        /* Same current progress+48 and CatData arguments used by native
           NPCMapDrawer setup at 279B60. Re-evaluate after each donation. */
        for (recipient = 0; recipient < 8; ++recipient)
            if (((AcceptsCat)(image + 0x276600))(progress + 0x48, recipient, data))
                accepts |= 1U << recipient;
        return AcMewSelectPopulationRecipient(accepts, ((const uint8_t*)data)[0x7ac] != 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

int AcMewChoosePopulationRecipient(void* scene, int64_t id, int recipient) {
    __try {
        const uint8_t* image = (const uint8_t*)GetModuleHandleW(NULL);
        typedef void (__fastcall *ChooseRecipient)(void*);
        static const uintptr_t chooser[] = {
            0x27bc20,0x27bec0,0x27b9a0,0x27c790,0x27c150,0x27c320,0x27c500
        };
        static const uint8_t choose_start[] = {0x48,0x89,0x5c,0x24,0x10,0x48,0x89};
        uint8_t* drawer;
        if (recipient < 0 || recipient > 7 ||
            AcMewFindPopulationRecipient(scene, id) != recipient) return 0;
        if (recipient == 7) return AcMewChooseTrashRecipient(scene, id);
        if (memcmp(image + chooser[recipient], choose_start, sizeof(choose_start)) ||
            image[chooser[recipient] + 7] != (recipient == 0 ? 0x74 : 0x7c) ||
            image[chooser[recipient] + 8] != 0x24 ||
            image[chooser[recipient] + 9] != 0x18) return 0;
        drawer = (uint8_t*)UniqueComponent(scene, "NPCMapDrawer");
        if (!drawer) return 0;
        ((ChooseRecipient)(image + chooser[recipient]))(drawer);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
