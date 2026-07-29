#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewHouseDetailResult {
    unsigned char signature_valid;
    unsigned char scene_valid;
    unsigned char house_unique;
    unsigned char cat_valid;
    unsigned char invoked;
} AcMewHouseDetailResult;

AcMewHouseDetailResult AcMewOpenHouseCatDetails(
    void* scene_manager,
    void* house_cat_component);

#ifdef __cplusplus
}
#endif
