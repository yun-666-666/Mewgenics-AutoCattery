#include "mew_ui_house_move_probe.h"
#include "mew_ui_house_move_room_scan.h"

#include <math.h>
#include <string.h>

static uint32_t AcCountChangedBytes(
    const uint8_t* before,
    const uint8_t* after,
    size_t byte_count) {
    uint32_t changed;
    size_t index;
    changed = 0U;
    for (index = 0U; index < byte_count; ++index) {
        if (before[index] != after[index]) {
            ++changed;
        }
    }
    return changed;
}

static int AcPlausibleDouble(double value) {
    const double magnitude = fabs(value);
    return isfinite(value) && magnitude <= 10000000.0 &&
           (magnitude == 0.0 || magnitude >= 0.000000001);
}

static void AcFindDoubleCandidates(
    const uint8_t* before,
    const uint8_t* after,
    size_t byte_count,
    uint8_t region,
    AcMewHouseMoveDiff* result) {
    size_t offset;
    if (!before || !after || !result) {
        return;
    }
    for (offset = 0U;
         offset + sizeof(double) * 3U <= byte_count &&
         result->double_candidate_count <
             AC_MEW_MOVE_PROBE_DOUBLE_CANDIDATES;
         offset += sizeof(double)) {
        AcMewMoveDoubleCandidate candidate;
        uint32_t index;
        int plausible;
        int changed;
        memset(&candidate, 0, sizeof(candidate));
        candidate.offset = (uint32_t)offset;
        candidate.region = region;
        memcpy(candidate.before, before + offset, sizeof(candidate.before));
        memcpy(candidate.after, after + offset, sizeof(candidate.after));
        plausible = 1;
        changed = 0;
        for (index = 0U; index < 3U; ++index) {
            plausible = plausible &&
                AcPlausibleDouble(candidate.before[index]) &&
                AcPlausibleDouble(candidate.after[index]);
            changed = changed ||
                fabs(candidate.before[index] - candidate.after[index]) >
                    0.000001;
        }
        if (plausible && changed) {
            result->double_candidates[result->double_candidate_count++] =
                candidate;
        }
    }
}

size_t AcMewCompareHouseMoveSamples(
    const AcMewHouseMoveSample* before,
    size_t before_count,
    const AcMewHouseMoveSample* after,
    size_t after_count,
    AcMewHouseMoveDiff* differences,
    size_t difference_capacity) {
    size_t before_index;
    size_t difference_count;
    if (!before || !after || !differences ||
        difference_capacity == 0U) {
        return 0U;
    }
    memset(differences, 0, difference_capacity * sizeof(*differences));
    difference_count = 0U;
    for (before_index = 0U;
         before_index < before_count &&
         difference_count < difference_capacity;
         ++before_index) {
        size_t after_index;
        const AcMewHouseMoveSample* first = &before[before_index];
        const AcMewHouseMoveSample* second = NULL;
        AcMewHouseMoveDiff candidate;
        for (after_index = 0U; after_index < after_count; ++after_index) {
            if (after[after_index].cat_id == first->cat_id &&
                after[after_index].component == first->component) {
                second = &after[after_index];
                break;
            }
        }
        if (!second || first->component_size == 0U ||
            first->component_size != second->component_size) {
            continue;
        }
        memset(&candidate, 0, sizeof(candidate));
        candidate.cat_id = first->cat_id;
        candidate.component_changed_bytes = AcCountChangedBytes(
            first->component_bytes,
            second->component_bytes,
            first->component_size);
        candidate.component_rooms_before = AcMewMoveRoomMask(
            first->component_bytes, first->component_size);
        candidate.component_rooms_after = AcMewMoveRoomMask(
            second->component_bytes, second->component_size);
        AcMewFindMoveRoomReferences(
            first->component_bytes,
            first->component_size,
            AC_MEW_MOVE_PROBE_COMPONENT,
            candidate.room_references_before,
            &candidate.room_references_before_count);
        AcMewFindMoveRoomReferences(
            second->component_bytes,
            second->component_size,
            AC_MEW_MOVE_PROBE_COMPONENT,
            candidate.room_references_after,
            &candidate.room_references_after_count);
        if (first->root_node == second->root_node &&
            first->root_size != 0U &&
            first->root_size == second->root_size) {
            candidate.root_changed_bytes = AcCountChangedBytes(
                first->root_bytes,
                second->root_bytes,
                first->root_size);
            candidate.root_rooms_before = AcMewMoveRoomMask(
                first->root_bytes, first->root_size);
            candidate.root_rooms_after = AcMewMoveRoomMask(
                second->root_bytes, second->root_size);
            AcMewFindMoveRoomReferences(
                first->root_bytes,
                first->root_size,
                AC_MEW_MOVE_PROBE_ROOT,
                candidate.room_references_before,
                &candidate.room_references_before_count);
            AcMewFindMoveRoomReferences(
                second->root_bytes,
                second->root_size,
                AC_MEW_MOVE_PROBE_ROOT,
                candidate.room_references_after,
                &candidate.room_references_after_count);
        }
        if (candidate.component_changed_bytes == 0U &&
            candidate.root_changed_bytes == 0U) {
            continue;
        }
        AcFindDoubleCandidates(
            first->component_bytes,
            second->component_bytes,
            first->component_size,
            AC_MEW_MOVE_PROBE_COMPONENT,
            &candidate);
        if (first->root_size != 0U &&
            first->root_size == second->root_size) {
            AcFindDoubleCandidates(
                first->root_bytes,
                second->root_bytes,
                first->root_size,
                AC_MEW_MOVE_PROBE_ROOT,
                &candidate);
        }
        differences[difference_count++] = candidate;
    }
    return difference_count;
}
