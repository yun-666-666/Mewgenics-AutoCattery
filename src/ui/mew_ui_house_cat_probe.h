#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewHouseCatMatch {
    int64_t cat_id;
    void* component;
    void* root_node;
} AcMewHouseCatMatch;

typedef struct AcMewHouseCatIdentityProbe {
    uint32_t house_cat_count;
    uint32_t requested_cat_count;
    uint32_t valid_layout_count;
    uint32_t first_identity_offset;
    uint8_t first_identity_width;
    uint8_t stable_bijection;
    uint8_t consistent_mapping;
    uint8_t reserved;
    size_t match_count;
} AcMewHouseCatIdentityProbe;

AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void* scene_manager,
    const int64_t* cat_ids,
    size_t cat_id_count,
    AcMewHouseCatMatch* matches,
    size_t match_capacity);

size_t AcMewCountHouseCats(void* scene_manager);

#ifdef __cplusplus
}
#endif
