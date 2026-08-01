#include "mew_ui_house_move_adapter.h"

#include <string.h>
#include <windows.h>

enum {
    AC_HOUSE_CAT_VTABLE_RVA = 0xEF4F58,
    AC_HOUSE_CAT_ROOM_OFFSET = 0xE8,
    AC_NATIVE_HOUSE_MOVE_RVA = 0x2E7DB0
};

typedef void (__fastcall *AcNativeHouseMoveFn)(
    void* target_room,
    void* house_cat);

static int AcReadable(const void* address, size_t size) {
    MEMORY_BASIC_INFORMATION memory;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    if (!address || size == 0U ||
        !VirtualQuery(address, &memory, sizeof(memory)) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return 0;
    }
    start = (uintptr_t)address;
    end = start + size;
    region_end =
        (uintptr_t)memory.BaseAddress + memory.RegionSize;
    return end >= start && end <= region_end;
}

static int AcValidHouseCat(void* house_cat, HMODULE executable) {
    if (!executable ||
        !AcReadable(house_cat, AC_HOUSE_CAT_ROOM_OFFSET + sizeof(void*))) {
        return 0;
    }
    return *(void**)house_cat ==
        (uint8_t*)executable + AC_HOUSE_CAT_VTABLE_RVA;
}

static int AcSignatureMatches(const uint8_t* address) {
    static const uint8_t expected[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x74, 0x24, 0x10,
        0x57, 0x48, 0x83, 0xEC, 0x20
    };
    return AcReadable(address, sizeof(expected)) &&
        memcmp(address, expected, sizeof(expected)) == 0;
}

static LONG AcCaptureException(
    EXCEPTION_POINTERS* exception,
    HMODULE executable,
    AcMewNativeHouseMoveResult* result) {
    if (exception && exception->ExceptionRecord && result) {
        result->seh_code = exception->ExceptionRecord->ExceptionCode;
        result->exception_rva =
            (uintptr_t)exception->ExceptionRecord->ExceptionAddress -
            (uintptr_t)executable;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void* AcMewReadHouseCatCurrentRoom(void* house_cat) {
    HMODULE executable = GetModuleHandleW(NULL);
    void* room;
    if (!AcValidHouseCat(house_cat, executable)) {
        return NULL;
    }
    room = *(void**)((uint8_t*)house_cat + AC_HOUSE_CAT_ROOM_OFFSET);
    return AcReadable(room, sizeof(void*)) ? room : NULL;
}

AcMewNativeHouseMoveResult AcMewInvokeNativeHouseMove(
    void* house_cat,
    void* target_room) {
    AcMewNativeHouseMoveResult result;
    HMODULE executable;
    uint8_t* native_move;
    memset(&result, 0, sizeof(result));
    executable = GetModuleHandleW(NULL);
    if (!executable) {
        return result;
    }
    native_move = (uint8_t*)executable + AC_NATIVE_HOUSE_MOVE_RVA;
    result.signature_valid = (uint8_t)AcSignatureMatches(native_move);
    result.cat_valid = (uint8_t)AcValidHouseCat(house_cat, executable);
    result.target_room_valid =
        (uint8_t)AcReadable(target_room, sizeof(void*));
    if (!result.signature_valid || !result.cat_valid ||
        !result.target_room_valid) {
        return result;
    }
    __try {
        ((AcNativeHouseMoveFn)native_move)(target_room, house_cat);
        result.invoked = 1U;
        result.committed = (uint8_t)(
            AcMewReadHouseCatCurrentRoom(house_cat) == target_room);
    } __except (AcCaptureException(
        GetExceptionInformation(), executable, &result)) {}
    return result;
}
