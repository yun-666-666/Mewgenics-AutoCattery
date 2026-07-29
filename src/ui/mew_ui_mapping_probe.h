#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_MAPPING_NAME_CAPACITY 96U

typedef struct AcMewAnonymousMappingObservation {
    uint32_t component_count;
    uint32_t typed_component_count;
    uint32_t type_name_count;
    uint32_t button_count;
    uint32_t role_count;
    uint64_t type_digest;
    uint64_t role_digest;
} AcMewAnonymousMappingObservation;

typedef struct AcMewMappingTypeRecord {
    char type_name[AC_MEW_MAPPING_NAME_CAPACITY];
    uint32_t component_count;
    uint32_t root_node_count;
} AcMewMappingTypeRecord;

typedef struct AcMewMappingRoleRecord {
    char role_name[AC_MEW_MAPPING_NAME_CAPACITY];
    uint32_t button_count;
} AcMewMappingRoleRecord;

AcMewAnonymousMappingObservation AcMewInspectAnonymousMapping(
    void* scene_manager);
size_t AcMewEnumerateMappingTypes(
    void* scene_manager,
    AcMewMappingTypeRecord* records,
    size_t record_capacity);
size_t AcMewEnumerateButtonRoles(
    void* scene_manager,
    AcMewMappingRoleRecord* records,
    size_t record_capacity);

#ifdef __cplusplus
}
#endif
