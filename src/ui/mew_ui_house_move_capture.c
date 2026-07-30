#include "mew_ui_house_move_probe.h"

#include <string.h>
#include <windows.h>

static int AcReadableRange(const void* pointer, size_t byte_count) {
    uintptr_t current;
    uintptr_t end;
    if (!pointer || byte_count == 0U) {
        return 0;
    }
    current = (uintptr_t)pointer;
    if (byte_count > UINTPTR_MAX - current) {
        return 0;
    }
    end = current + byte_count;
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
            return 0;
        }
        region_end = (uintptr_t)info.BaseAddress +
                     (uintptr_t)info.RegionSize;
        if (region_end <= current) {
            return 0;
        }
        current = region_end < end ? region_end : end;
    }
    return 1;
}

static size_t AcReadablePrefix(
    const void* pointer,
    size_t capacity) {
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
    if (!destination || !AcReadableRange(source, byte_count)) {
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

size_t AcMewCaptureHouseMoveSamples(
    const AcMewHouseCatMatch* matches,
    size_t match_count,
    AcMewHouseMoveSample* samples,
    size_t sample_capacity) {
    size_t index;
    size_t captured;
    if (!matches || !samples || match_count == 0U ||
        sample_capacity < match_count) {
        return 0U;
    }
    memset(samples, 0, match_count * sizeof(*samples));
    captured = 0U;
    for (index = 0U; index < match_count; ++index) {
        AcMewHouseMoveSample* sample = &samples[captured];
        const size_t component_size = AcReadablePrefix(
            matches[index].component,
            sizeof(sample->component_bytes));
        if (matches[index].cat_id == 0 || !matches[index].component ||
            component_size < 0x200U ||
            !AcCopyReadable(
                matches[index].component,
                sample->component_bytes,
                component_size)) {
            continue;
        }
        sample->cat_id = matches[index].cat_id;
        sample->component = matches[index].component;
        sample->root_node = matches[index].root_node;
        sample->component_size = (uint32_t)component_size;
        if (matches[index].root_node) {
            const size_t root_size = AcReadablePrefix(
                matches[index].root_node,
                sizeof(sample->root_bytes));
            if (root_size >= 0x80U &&
                AcCopyReadable(
                    matches[index].root_node,
                    sample->root_bytes,
                    root_size)) {
                sample->root_size = (uint32_t)root_size;
            }
        }
        ++captured;
    }
    return captured;
}
