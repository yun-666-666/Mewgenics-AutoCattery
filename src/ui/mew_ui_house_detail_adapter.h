#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewHouseDetailResult {
    unsigned char signature_valid;
    unsigned char scene_valid;
    unsigned char click_manager_valid;
    unsigned char drawer_unique;
    unsigned char scene_drawer_unique;
    unsigned char drawer_matches_scene;
    unsigned char cat_valid;
    unsigned char detail_target_valid;
    unsigned char invoked;
    unsigned char failure_stage;
    unsigned long seh_code;
    unsigned long long exception_rva;
} AcMewHouseDetailResult;

AcMewHouseDetailResult AcMewOpenHouseCatDetails(
    void* scene_manager,
    void* house_cat_component);

#ifdef __cplusplus
}
#endif
