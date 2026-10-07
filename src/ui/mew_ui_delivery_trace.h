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
    uint32_t npc_drawer_active;
    double completion_delay;
    uint8_t open;
    uint8_t cat_mode;
    int32_t npc_result;
    int64_t cat_id;
    uintptr_t callback_rva;
} AcDeliveryTrace;
AcDeliveryTrace AcMewReadDeliveryTrace(void* scene);
int AcMewOpenDeadCatPipe(void* scene, void* cat, int64_t cat_id);
int AcMewChooseDeadCatRecipient(void* scene, int64_t cat_id);
int AcMewOpenPopulationCatPipe(void* scene, void* cat, int64_t cat_id);
int AcMewChooseTrashRecipient(void* scene, int64_t cat_id);
/* -1 means unavailable, 0..6 a currently accepting NPC, 7 trash only. */
int AcMewFindPopulationRecipient(void* scene, int64_t cat_id);
int AcMewChoosePopulationRecipient(void* scene, int64_t cat_id, int recipient);
#ifdef __cplusplus
}
#endif
