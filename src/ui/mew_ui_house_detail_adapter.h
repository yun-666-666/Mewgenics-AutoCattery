#pragma once

#include <stddef.h>
#include <stdint.h>

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

typedef struct AcMewHouseDetailLayout {
    uintptr_t open_cat_details_rva;
    uintptr_t cat_detail_target_rva;
    uintptr_t drawer_from_click_manager_rva;
} AcMewHouseDetailLayout;

int AcMewSelectHouseDetailLayout(
    const uint8_t* image,
    size_t image_size,
    AcMewHouseDetailLayout* output);

AcMewHouseDetailResult AcMewOpenHouseCatDetails(
    void* scene_manager,
    void* house_cat_component);

#ifdef __cplusplus
}
#endif
