#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_SCENE_NAME_CAPACITY 128U

typedef struct AcMewSceneRecord {
    void* scene_manager;
    char scene_name[AC_MEW_SCENE_NAME_CAPACITY];
    uint32_t component_count;
    uint8_t ready;
} AcMewSceneRecord;

size_t AcMewEnumerateScenes(
    AcMewSceneRecord* records,
    size_t record_capacity);

int AcMewSceneHasComponentType(
    void* scene_manager,
    const char* component_type_name);

void* AcMewFindComponentByType(
    void* scene_manager,
    const char* component_type_name);

int AcMewFurnitureBuildingUiIsActive(void* component);

#ifdef __cplusplus
}
#endif
