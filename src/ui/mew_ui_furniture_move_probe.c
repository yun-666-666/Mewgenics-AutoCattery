#include "mew_ui_furniture_move_probe.h"

#include <math.h>
#include <string.h>
#include <windows.h>

enum {
    AC_MEW_FURNITURE_PROBE_MAX_DEPTH = 2
};

static int AcModuleBounds(uint64_t* base, uint32_t* size) {
    HMODULE module;
    IMAGE_DOS_HEADER* dos;
    IMAGE_NT_HEADERS64* nt;
    if (!base || !size) {
        return 0;
    }
    module = GetModuleHandleW(NULL);
    if (!module) {
        return 0;
    }
    dos = (IMAGE_DOS_HEADER*)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    nt = (IMAGE_NT_HEADERS64*)((uint8_t*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return 0;
    }
    *base = (uint64_t)(uintptr_t)module;
    *size = nt->OptionalHeader.SizeOfImage;
    return *size != 0U;
}

static int AcInsideModule(
    uint64_t value,
    uint64_t module_base,
    uint32_t module_size) {
    return value >= module_base &&
        value - module_base < (uint64_t)module_size;
}

static size_t AcReadablePrefix(const void* pointer, size_t capacity) {
    uintptr_t current;
    uintptr_t end;
    if (!pointer || capacity == 0U) {
        return 0U;
    }
    current = (uintptr_t)pointer;
    if (capacity > UINTPTR_MAX - current) {
        return 0U;
    }
    end = current + capacity;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t region_end;
        const DWORD blocked = PAGE_GUARD | PAGE_NOACCESS;
        if (VirtualQuery(
                (const void*)current,
                &info,
                sizeof(info)) != sizeof(info) ||
            info.State != MEM_COMMIT ||
            (info.Protect & blocked) != 0U) {
            break;
        }
        region_end = (uintptr_t)info.BaseAddress +
            (uintptr_t)info.RegionSize;
        if (region_end <= current) {
            break;
        }
        current = region_end < end ? region_end : end;
    }
    return (size_t)(current - (uintptr_t)pointer);
}

static int AcCopyReadable(
    const void* source,
    void* destination,
    size_t byte_count) {
    if (!source || !destination || byte_count == 0U ||
        AcReadablePrefix(source, byte_count) < byte_count) {
        return 0;
    }
    __try {
        memcpy(destination, source, byte_count);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static uint64_t AcReadVtableRva(
    uint64_t address,
    uint64_t module_base,
    uint32_t module_size) {
    uint64_t first;
    first = 0U;
    if (!address ||
        !AcCopyReadable(
            (const void*)(uintptr_t)address,
            &first,
            sizeof(first)) ||
        !AcInsideModule(first, module_base, module_size)) {
        return 0U;
    }
    return first - module_base;
}

static int AcContainsNode(
    const AcMewFurnitureMoveSample* sample,
    uint64_t address) {
    uint32_t index;
    for (index = 0U; index < sample->node_count; ++index) {
        if (sample->nodes[index].address == address) {
            return 1;
        }
    }
    return 0;
}

static int AcAddNode(
    AcMewFurnitureMoveSample* sample,
    uint64_t address,
    uint64_t parent_address,
    uint32_t parent_offset,
    uint16_t depth) {
    AcMewFurnitureProbeNode* node;
    size_t readable;
    if (!sample || !address ||
        sample->node_count >= AC_MEW_FURNITURE_PROBE_NODES ||
        AcContainsNode(sample, address)) {
        return 0;
    }
    readable = AcReadablePrefix(
        (const void*)(uintptr_t)address,
        AC_MEW_FURNITURE_PROBE_NODE_BYTES);
    if (readable < 0x40U) {
        return 0;
    }
    node = &sample->nodes[sample->node_count];
    memset(node, 0, sizeof(*node));
    node->address = address;
    node->parent_address = parent_address;
    node->parent_offset = parent_offset;
    node->depth = depth;
    node->byte_count = (uint32_t)readable;
    node->vtable_rva = AcReadVtableRva(
        address,
        sample->module_base,
        sample->module_size);
    if (!AcCopyReadable(
            (const void*)(uintptr_t)address,
            node->bytes,
            readable)) {
        memset(node, 0, sizeof(*node));
        return 0;
    }
    ++sample->node_count;
    return 1;
}

int AcMewCaptureFurnitureMoveSample(
    void* root,
    AcMewFurnitureMoveSample* sample) {
    uint32_t node_index;
    if (!root || !sample) {
        return 0;
    }
    memset(sample, 0, sizeof(*sample));
    if (!AcModuleBounds(&sample->module_base, &sample->module_size) ||
        !AcAddNode(
            sample,
            (uint64_t)(uintptr_t)root,
            0U,
            0U,
            0U)) {
        return 0;
    }
    for (node_index = 0U;
         node_index < sample->node_count &&
         sample->node_count < AC_MEW_FURNITURE_PROBE_NODES;
         ++node_index) {
        AcMewFurnitureProbeNode* node = &sample->nodes[node_index];
        uint32_t offset;
        if (node->depth >= AC_MEW_FURNITURE_PROBE_MAX_DEPTH) {
            continue;
        }
        for (offset = 0U;
             offset + sizeof(uint64_t) <= node->byte_count &&
             sample->node_count < AC_MEW_FURNITURE_PROBE_NODES;
             offset += (uint32_t)sizeof(uint64_t)) {
            uint64_t candidate;
            candidate = 0U;
            memcpy(&candidate, node->bytes + offset, sizeof(candidate));
            if (candidate < 0x10000U ||
                (candidate & (sizeof(void*) - 1U)) != 0U ||
                AcInsideModule(
                    candidate,
                    sample->module_base,
                    sample->module_size) ||
                AcReadablePrefix(
                    (const void*)(uintptr_t)candidate,
                    0x40U) < 0x40U) {
                continue;
            }
            (void)AcAddNode(
                sample,
                candidate,
                node->address,
                offset,
                (uint16_t)(node->depth + 1U));
        }
    }
    return sample->node_count != 0U;
}

static const AcMewFurnitureProbeNode* AcFindNode(
    const AcMewFurnitureMoveSample* sample,
    uint64_t address) {
    uint32_t index;
    if (!sample) {
        return NULL;
    }
    for (index = 0U; index < sample->node_count; ++index) {
        if (sample->nodes[index].address == address) {
            return &sample->nodes[index];
        }
    }
    return NULL;
}

static int AcWindowChanged(
    const uint8_t* before,
    const uint8_t* after,
    uint32_t offset,
    uint32_t width) {
    return memcmp(before + offset, after + offset, width) != 0;
}

static int AcPlausibleInt32(int32_t value) {
    return value >= -100000000 && value <= 100000000;
}

static int AcPlausibleDouble(double value) {
    return isfinite(value) && fabs(value) <= 100000000.0;
}

static int AcPointerLike(
    uint64_t value,
    uint64_t module_base,
    uint32_t module_size) {
    if (value == 0U ||
        AcInsideModule(value, module_base, module_size)) {
        return 1;
    }
    return value >= 0x10000U &&
        (value & (sizeof(void*) - 1U)) == 0U &&
        AcReadablePrefix((const void*)(uintptr_t)value, sizeof(void*)) ==
            sizeof(void*);
}

static uint32_t AcChangedBytes(
    const AcMewFurnitureProbeNode* before,
    const AcMewFurnitureProbeNode* after) {
    uint32_t count;
    uint32_t index;
    uint32_t shared;
    count = 0U;
    shared = before->byte_count < after->byte_count
        ? before->byte_count : after->byte_count;
    for (index = 0U; index < shared; ++index) {
        if (before->bytes[index] != after->bytes[index]) {
            ++count;
        }
    }
    count += before->byte_count > after->byte_count
        ? before->byte_count - after->byte_count
        : after->byte_count - before->byte_count;
    return count;
}

static void AcAddChangedRanges(
    const AcMewFurnitureProbeNode* before,
    const AcMewFurnitureProbeNode* after,
    AcMewFurnitureProbeNodeDiff* difference) {
    uint32_t index;
    uint32_t shared;
    shared = before->byte_count < after->byte_count
        ? before->byte_count : after->byte_count;
    index = 0U;
    while (index < shared &&
           difference->range_count < AC_MEW_FURNITURE_PROBE_RANGES) {
        uint32_t start;
        if (before->bytes[index] == after->bytes[index]) {
            ++index;
            continue;
        }
        start = index;
        while (index < shared &&
               before->bytes[index] != after->bytes[index]) {
            ++index;
        }
        difference->ranges[difference->range_count].offset = start;
        difference->ranges[difference->range_count].length = index - start;
        ++difference->range_count;
    }
    if (before->byte_count != after->byte_count &&
        difference->range_count < AC_MEW_FURNITURE_PROBE_RANGES) {
        difference->ranges[difference->range_count].offset = shared;
        difference->ranges[difference->range_count].length =
            before->byte_count > after->byte_count
                ? before->byte_count - shared
                : after->byte_count - shared;
        ++difference->range_count;
    }
}

static void AcAddNumericCandidates(
    const AcMewFurnitureMoveSample* sample,
    const AcMewFurnitureProbeNode* before,
    const AcMewFurnitureProbeNode* after,
    AcMewFurnitureProbeNodeDiff* difference) {
    uint32_t offset;
    uint32_t shared;
    shared = before->byte_count < after->byte_count
        ? before->byte_count : after->byte_count;
    for (offset = 0U;
         offset + sizeof(int32_t) <= shared &&
         difference->int32_candidate_count <
             AC_MEW_FURNITURE_PROBE_INT32_CANDIDATES;
         offset += (uint32_t)sizeof(int32_t)) {
        int32_t first;
        int32_t second;
        if (!AcWindowChanged(
                before->bytes,
                after->bytes,
                offset,
                sizeof(int32_t))) {
            continue;
        }
        memcpy(&first, before->bytes + offset, sizeof(first));
        memcpy(&second, after->bytes + offset, sizeof(second));
        if (first == second || !AcPlausibleInt32(first) ||
            !AcPlausibleInt32(second)) {
            continue;
        }
        difference->int32_candidates[
            difference->int32_candidate_count].offset = offset;
        difference->int32_candidates[
            difference->int32_candidate_count].before = first;
        difference->int32_candidates[
            difference->int32_candidate_count].after = second;
        ++difference->int32_candidate_count;
    }
    for (offset = 0U;
         offset + sizeof(double) <= shared &&
         difference->double_candidate_count <
             AC_MEW_FURNITURE_PROBE_DOUBLE_CANDIDATES;
         offset += (uint32_t)sizeof(double)) {
        double first;
        double second;
        if (!AcWindowChanged(
                before->bytes,
                after->bytes,
                offset,
                sizeof(double))) {
            continue;
        }
        memcpy(&first, before->bytes + offset, sizeof(first));
        memcpy(&second, after->bytes + offset, sizeof(second));
        if (first == second || !AcPlausibleDouble(first) ||
            !AcPlausibleDouble(second)) {
            continue;
        }
        difference->double_candidates[
            difference->double_candidate_count].offset = offset;
        difference->double_candidates[
            difference->double_candidate_count].before = first;
        difference->double_candidates[
            difference->double_candidate_count].after = second;
        ++difference->double_candidate_count;
    }
    for (offset = 0U;
         offset + sizeof(uint64_t) <= shared &&
         difference->pointer_candidate_count <
             AC_MEW_FURNITURE_PROBE_POINTER_CANDIDATES;
         offset += (uint32_t)sizeof(uint64_t)) {
        uint64_t first;
        uint64_t second;
        if (!AcWindowChanged(
                before->bytes,
                after->bytes,
                offset,
                sizeof(uint64_t))) {
            continue;
        }
        memcpy(&first, before->bytes + offset, sizeof(first));
        memcpy(&second, after->bytes + offset, sizeof(second));
        if (first == second ||
            !AcPointerLike(
                first,
                sample->module_base,
                sample->module_size) ||
            !AcPointerLike(
                second,
                sample->module_base,
                sample->module_size)) {
            continue;
        }
        difference->pointer_candidates[
            difference->pointer_candidate_count].offset = offset;
        difference->pointer_candidates[
            difference->pointer_candidate_count].before = first;
        difference->pointer_candidates[
            difference->pointer_candidate_count].after = second;
        difference->pointer_candidates[
            difference->pointer_candidate_count].before_target_vtable_rva =
                AcReadVtableRva(
                    first,
                    sample->module_base,
                    sample->module_size);
        difference->pointer_candidates[
            difference->pointer_candidate_count].after_target_vtable_rva =
                AcReadVtableRva(
                    second,
                    sample->module_base,
                    sample->module_size);
        ++difference->pointer_candidate_count;
    }
}

static void AcPopulateNodeDiff(
    const AcMewFurnitureMoveSample* sample,
    const AcMewFurnitureProbeNode* before,
    const AcMewFurnitureProbeNode* after,
    uint8_t status,
    AcMewFurnitureProbeNodeDiff* difference) {
    const AcMewFurnitureProbeNode* source = before ? before : after;
    memset(difference, 0, sizeof(*difference));
    difference->status = status;
    difference->address = source->address;
    difference->parent_address = source->parent_address;
    difference->parent_offset = source->parent_offset;
    difference->depth = source->depth;
    difference->before_vtable_rva = before ? before->vtable_rva : 0U;
    difference->after_vtable_rva = after ? after->vtable_rva : 0U;
    difference->before_byte_count = before ? before->byte_count : 0U;
    difference->after_byte_count = after ? after->byte_count : 0U;
    if (!before || !after) {
        difference->changed_bytes = source->byte_count;
        return;
    }
    difference->changed_bytes = AcChangedBytes(before, after);
    AcAddChangedRanges(before, after, difference);
    AcAddNumericCandidates(sample, before, after, difference);
}

void AcMewCompareFurnitureMoveSamples(
    const AcMewFurnitureMoveSample* before,
    const AcMewFurnitureMoveSample* after,
    uint8_t root_kind,
    AcMewFurnitureMoveDiff* difference) {
    uint32_t index;
    if (!before || !after || !difference) {
        return;
    }
    memset(difference, 0, sizeof(*difference));
    difference->root_kind = root_kind;
    difference->before_node_count = before->node_count;
    difference->after_node_count = after->node_count;
    for (index = 0U;
         index < before->node_count &&
         difference->changed_node_count < AC_MEW_FURNITURE_PROBE_NODES;
         ++index) {
        const AcMewFurnitureProbeNode* first = &before->nodes[index];
        const AcMewFurnitureProbeNode* second =
            AcFindNode(after, first->address);
        if (!second) {
            AcPopulateNodeDiff(
                before,
                first,
                NULL,
                AC_MEW_FURNITURE_PROBE_NODE_DISAPPEARED,
                &difference->changed_nodes[
                    difference->changed_node_count++]);
            continue;
        }
        if (AcChangedBytes(first, second) == 0U &&
            first->vtable_rva == second->vtable_rva) {
            continue;
        }
        AcPopulateNodeDiff(
            before,
            first,
            second,
            AC_MEW_FURNITURE_PROBE_NODE_CHANGED,
            &difference->changed_nodes[
                difference->changed_node_count++]);
    }
    for (index = 0U;
         index < after->node_count &&
         difference->changed_node_count < AC_MEW_FURNITURE_PROBE_NODES;
         ++index) {
        const AcMewFurnitureProbeNode* second = &after->nodes[index];
        if (AcFindNode(before, second->address)) {
            continue;
        }
        AcPopulateNodeDiff(
            after,
            NULL,
            second,
            AC_MEW_FURNITURE_PROBE_NODE_APPEARED,
            &difference->changed_nodes[
                difference->changed_node_count++]);
    }
}
