#include "mew_ui_live_house_state.h"

#include <string.h>
#include <windows.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"
#include "mew_ui_house_move_adapter.h"
#include "mew_ui_scene_components.h"

#define AC_VERIFIED_CAT_ID_OFFSET 0x80U

static int AcTypeEqualsHouseCat(void* component) {
    MewComponent* typed;
    MewNarrowString name;
    const char* data;
    size_t size;
    if (!component) {
        return 0;
    }
    typed = (MewComponent*)component;
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        memset(&name, 0, sizeof(name));
        typed->vtable->GetObjectTypeSTR(component, &name);
        data = MewUI_GetNarrowStringData(&name);
        size = MewUI_GetNarrowStringSize(&name);
        return data && size == 8U && memcmp(data, "HouseCat", 8U) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcCaptureCat(
    void* component,
    AcMewLiveHouseCatState* state) {
    int64_t cat_id;
    void* room;
    if (!component || !state) {
        return 0;
    }
    __try {
        memcpy(
            &cat_id,
            (const uint8_t*)component + AC_VERIFIED_CAT_ID_OFFSET,
            sizeof(cat_id));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    room = AcMewReadHouseCatCurrentRoom(component);
    if (cat_id <= 0) {
        return 0;
    }
    state->cat_id = cat_id;
    state->component = component;
    state->room = room;
    return 1;
}

static int AcDuplicateId(
    const AcMewLiveHouseCatState* states,
    size_t count,
    int64_t cat_id) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (states[index].cat_id == cat_id) {
            return 1;
        }
    }
    return 0;
}

size_t AcMewCaptureLiveHouseCats(
    void* scene_manager,
    AcMewLiveHouseCatState* states,
    size_t state_capacity) {
    MewPodVectorPtr* components;
    size_t captured;
    uint32_t index;
    if (!scene_manager || !states || state_capacity == 0U) {
        return 0U;
    }
    memset(states, 0, state_capacity * sizeof(*states));
    components = AcMewGetValidatedSceneComponents(scene_manager);
    if (!components) {
        return 0U;
    }
    captured = 0U;
    for (index = 0U; index < components->size; ++index) {
        AcMewLiveHouseCatState state;
        void* component = components->data[index];
        if (!AcTypeEqualsHouseCat(component)) {
            continue;
        }
        if (captured >= state_capacity ||
            !AcCaptureCat(component, &state) ||
            AcDuplicateId(states, captured, state.cat_id)) {
            return 0U;
        }
        states[captured++] = state;
    }
    return captured;
}
