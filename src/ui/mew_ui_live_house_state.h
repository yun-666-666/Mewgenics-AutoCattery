#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewLiveHouseCatState {
    int64_t cat_id;
    void* component;
    void* room;
} AcMewLiveHouseCatState;

size_t AcMewCaptureLiveHouseCats(
    void* scene_manager,
    AcMewLiveHouseCatState* states,
    size_t state_capacity);

#ifdef __cplusplus
}
#endif
