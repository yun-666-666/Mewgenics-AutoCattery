#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcPoopCleanupResult {
    uint32_t cleaned;
    int completed;
    uint32_t seh_code;
} AcPoopCleanupResult;

/* The native pop path defers component destruction until the scene tick. */
typedef void (*AcPoopPopFn)(void*);
AcPoopCleanupResult AcMewCleanPoopComponents(
    void* const* components, size_t count, const void* furniture_vtable,
    AcPoopPopFn pop);
AcPoopCleanupResult AcMewCleanHousePoop(void* scene_manager);

#ifdef __cplusplus
}
#endif
