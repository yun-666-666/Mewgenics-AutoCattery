#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MEW_FURNITURE_PROBE_NODE_BYTES 0x600U
#define AC_MEW_FURNITURE_PROBE_NODES 96U
#define AC_MEW_FURNITURE_PROBE_RANGES 32U
#define AC_MEW_FURNITURE_PROBE_INT32_CANDIDATES 32U
#define AC_MEW_FURNITURE_PROBE_DOUBLE_CANDIDATES 32U
#define AC_MEW_FURNITURE_PROBE_POINTER_CANDIDATES 24U

typedef enum AcMewFurnitureProbeRootKind {
    AC_MEW_FURNITURE_PROBE_UI = 1,
    AC_MEW_FURNITURE_PROBE_HOUSE_SCENE = 2,
    AC_MEW_FURNITURE_PROBE_HOUSE_INVENTORY = 3,
    AC_MEW_FURNITURE_PROBE_EDITOR = 4,
    AC_MEW_FURNITURE_PROBE_CLICK_HANDLER = 5
} AcMewFurnitureProbeRootKind;

typedef enum AcMewFurnitureProbeNodeStatus {
    AC_MEW_FURNITURE_PROBE_NODE_CHANGED = 0,
    AC_MEW_FURNITURE_PROBE_NODE_APPEARED = 1,
    AC_MEW_FURNITURE_PROBE_NODE_DISAPPEARED = 2
} AcMewFurnitureProbeNodeStatus;

typedef struct AcMewFurnitureProbeNode {
    uint64_t address;
    uint64_t vtable_rva;
    uint64_t parent_address;
    uint32_t parent_offset;
    uint32_t byte_count;
    uint16_t depth;
    uint16_t reserved;
    uint8_t bytes[AC_MEW_FURNITURE_PROBE_NODE_BYTES];
} AcMewFurnitureProbeNode;

typedef struct AcMewFurnitureMoveSample {
    uint64_t module_base;
    uint32_t module_size;
    uint32_t node_count;
    AcMewFurnitureProbeNode nodes[AC_MEW_FURNITURE_PROBE_NODES];
} AcMewFurnitureMoveSample;

typedef struct AcMewFurnitureProbeByteRange {
    uint32_t offset;
    uint32_t length;
} AcMewFurnitureProbeByteRange;

typedef struct AcMewFurnitureProbeInt32Candidate {
    uint32_t offset;
    int32_t before;
    int32_t after;
} AcMewFurnitureProbeInt32Candidate;

typedef struct AcMewFurnitureProbeDoubleCandidate {
    uint32_t offset;
    uint32_t reserved;
    double before;
    double after;
} AcMewFurnitureProbeDoubleCandidate;

typedef struct AcMewFurnitureProbePointerCandidate {
    uint32_t offset;
    uint32_t reserved;
    uint64_t before;
    uint64_t after;
    uint64_t before_target_vtable_rva;
    uint64_t after_target_vtable_rva;
} AcMewFurnitureProbePointerCandidate;

typedef struct AcMewFurnitureProbeNodeDiff {
    uint64_t address;
    uint64_t before_vtable_rva;
    uint64_t after_vtable_rva;
    uint64_t parent_address;
    uint32_t parent_offset;
    uint32_t before_byte_count;
    uint32_t after_byte_count;
    uint32_t changed_bytes;
    uint16_t depth;
    uint8_t status;
    uint8_t reserved;
    uint32_t range_count;
    uint32_t int32_candidate_count;
    uint32_t double_candidate_count;
    uint32_t pointer_candidate_count;
    AcMewFurnitureProbeByteRange ranges[AC_MEW_FURNITURE_PROBE_RANGES];
    AcMewFurnitureProbeInt32Candidate
        int32_candidates[AC_MEW_FURNITURE_PROBE_INT32_CANDIDATES];
    AcMewFurnitureProbeDoubleCandidate
        double_candidates[AC_MEW_FURNITURE_PROBE_DOUBLE_CANDIDATES];
    AcMewFurnitureProbePointerCandidate
        pointer_candidates[AC_MEW_FURNITURE_PROBE_POINTER_CANDIDATES];
} AcMewFurnitureProbeNodeDiff;

typedef struct AcMewFurnitureMoveDiff {
    uint8_t root_kind;
    uint8_t reserved[3];
    uint32_t before_node_count;
    uint32_t after_node_count;
    uint32_t changed_node_count;
    AcMewFurnitureProbeNodeDiff
        changed_nodes[AC_MEW_FURNITURE_PROBE_NODES];
} AcMewFurnitureMoveDiff;

int AcMewCaptureFurnitureMoveSample(
    void* root,
    AcMewFurnitureMoveSample* sample);

void AcMewCompareFurnitureMoveSamples(
    const AcMewFurnitureMoveSample* before,
    const AcMewFurnitureMoveSample* after,
    uint8_t root_kind,
    AcMewFurnitureMoveDiff* difference);

#ifdef __cplusplus
}
#endif
