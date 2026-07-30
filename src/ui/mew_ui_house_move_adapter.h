#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcMewNativeHouseMoveResult {
    uint8_t signature_valid;
    uint8_t cat_valid;
    uint8_t target_room_valid;
    uint8_t invoked;
    uint8_t committed;
    uint8_t reserved[3];
    uint32_t seh_code;
    uintptr_t exception_rva;
} AcMewNativeHouseMoveResult;

void* AcMewReadHouseCatCurrentRoom(void* house_cat);

size_t AcMewEnumerateNativeHouseRooms(
    void* house_scene_manager,
    void** rooms,
    size_t room_capacity);

uint32_t AcMewDetectNativeHouseRoomMask(void* room);

AcMewNativeHouseMoveResult AcMewInvokeNativeHouseMove(
    void* house_cat,
    void* target_room);

#ifdef __cplusplus
}
#endif
