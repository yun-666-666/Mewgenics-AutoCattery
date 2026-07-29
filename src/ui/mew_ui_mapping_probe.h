#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewAnonymousMappingObservation {
    uint32_t component_count;
    uint32_t typed_component_count;
    uint32_t type_name_count;
    uint32_t button_count;
    uint32_t role_count;
    uint64_t type_digest;
    uint64_t role_digest;
} AcMewAnonymousMappingObservation;

AcMewAnonymousMappingObservation AcMewInspectAnonymousMapping(
    void* scene_manager);

#ifdef __cplusplus
}
#endif
