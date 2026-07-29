#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_HOUSE_CAT_MATCH_CAPACITY 64U

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
    AcMewHouseCatMatch matches[AC_MEW_HOUSE_CAT_MATCH_CAPACITY];
} AcMewHouseCatIdentityProbe;

AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void* scene_manager,
    const int64_t* cat_ids,
    size_t cat_id_count);

#ifdef __cplusplus
}
#endif
