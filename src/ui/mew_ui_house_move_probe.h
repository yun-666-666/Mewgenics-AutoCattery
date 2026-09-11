#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mew_ui_house_cat_probe.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_MOVE_PROBE_COMPONENT_BYTES 0x2000U
#define AC_MEW_MOVE_PROBE_ROOT_BYTES 0x800U
#define AC_MEW_MOVE_PROBE_ROOM_COUNT 6U
#define AC_MEW_MOVE_PROBE_DOUBLE_CANDIDATES 48U
#define AC_MEW_MOVE_PROBE_ROOM_REFERENCES 16U

typedef enum AcMewMoveProbeRegion {
    AC_MEW_MOVE_PROBE_COMPONENT = 1,
    AC_MEW_MOVE_PROBE_ROOT = 2
} AcMewMoveProbeRegion;

typedef struct AcMewHouseMoveSample {
    int64_t cat_id;
    void* component;
    void* root_node;
    uint32_t component_size;
    uint32_t root_size;
    uint8_t component_bytes[AC_MEW_MOVE_PROBE_COMPONENT_BYTES];
    uint8_t root_bytes[AC_MEW_MOVE_PROBE_ROOT_BYTES];
} AcMewHouseMoveSample;

typedef struct AcMewMoveDoubleCandidate {
    uint32_t offset;
    uint8_t region;
    uint8_t reserved[3];
    double before[3];
    double after[3];
} AcMewMoveDoubleCandidate;

typedef struct AcMewMoveRoomReference {
    uint32_t offset;
    uint8_t region;
    uint8_t room_index;
    uint8_t pointer_reference;
    uint8_t reserved;
} AcMewMoveRoomReference;

typedef struct AcMewHouseMoveDiff {
    int64_t cat_id;
    uint32_t component_changed_bytes;
    uint32_t root_changed_bytes;
    uint32_t component_rooms_before;
    uint32_t component_rooms_after;
    uint32_t root_rooms_before;
    uint32_t root_rooms_after;
    uint32_t room_references_before_count;
    uint32_t room_references_after_count;
    uint32_t double_candidate_count;
    AcMewMoveRoomReference
        room_references_before[AC_MEW_MOVE_PROBE_ROOM_REFERENCES];
    AcMewMoveRoomReference
        room_references_after[AC_MEW_MOVE_PROBE_ROOM_REFERENCES];
    AcMewMoveDoubleCandidate
        double_candidates[AC_MEW_MOVE_PROBE_DOUBLE_CANDIDATES];
} AcMewHouseMoveDiff;

size_t AcMewCaptureHouseMoveSamples(
    const AcMewHouseCatMatch* matches,
    size_t match_count,
    AcMewHouseMoveSample* samples,
    size_t sample_capacity);

size_t AcMewCompareHouseMoveSamples(
    const AcMewHouseMoveSample* before,
    size_t before_count,
    const AcMewHouseMoveSample* after,
    size_t after_count,
    AcMewHouseMoveDiff* differences,
    size_t difference_capacity);

const char* AcMewMoveProbeRoomId(uint32_t room_index);

#ifdef __cplusplus
}
#endif
