#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AcDeliveryTrace {
    uint32_t layout_valid;
    uint32_t drawer_count;
    uint32_t house_cat_count;
    uint32_t selected_valid;
    uint32_t callback_bound_to_drawer;
    uint32_t seh_code;
    uint8_t open;
    uint8_t cat_mode;
    int32_t npc_result;
    int64_t cat_id;
    uintptr_t callback_rva;
} AcDeliveryTrace;
AcDeliveryTrace AcMewReadDeliveryTrace(void* scene);
int AcMewOpenDeadCatPipe(void* scene, void* cat, int64_t cat_id);
int AcMewChooseDeadCatRecipient(void* scene, int64_t cat_id);
int AcMewCloseDeliveryDetails(void* scene);
#ifdef __cplusplus
}
#endif
