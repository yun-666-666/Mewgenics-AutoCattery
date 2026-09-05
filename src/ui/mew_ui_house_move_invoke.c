#include "mew_ui_house_move_adapter.h"

#include <string.h>
#include <windows.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

enum {
    AC_HOUSE_CAT_ROOM_OFFSET = 0xE8,
    AC_NATIVE_HOUSE_MOVE_STABLE_RVA = 0x2E7DB0,
    AC_NATIVE_HOUSE_MOVE_BETA_RVA = 0x2E88D0
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

static size_t AcExecutableImageSize(HMODULE executable) {
    if (!executable) {
        return 0U;
    }
    __try {
        const uint8_t* image = (const uint8_t*)executable;
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)image;
        const IMAGE_NT_HEADERS64* nt;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
            return 0U;
        }
        nt = (const IMAGE_NT_HEADERS64*)(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return 0U;
        }
        return nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
}

static int AcTypeEqualsHouseCat(void* component) {
    MewComponent* typed;
    MewNarrowString name;
    const char* data;
    size_t size;
    if (!component) {
        return 0;
    }
    typed = (MewComponent*)component;
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        memset(&name, 0, sizeof(name));
        typed->vtable->GetObjectTypeSTR(component, &name);
        data = MewUI_GetNarrowStringData(&name);
        size = MewUI_GetNarrowStringSize(&name);
        return data && size == 8U && memcmp(data, "HouseCat", 8U) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcValidHouseCat(void* house_cat) {
    return AcReadable(
               house_cat,
               AC_HOUSE_CAT_ROOM_OFFSET + sizeof(void*)) &&
           AcTypeEqualsHouseCat(house_cat);
}

static int AcSignatureMatches(
    const uint8_t* image,
    size_t image_size,
    uintptr_t rva,
    size_t signature_size) {
    static const uint8_t expected[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x74, 0x24, 0x10,
        0x57, 0x48, 0x83, 0xEC, 0x20,
        0x48, 0x8B, 0xF9, 0x48, 0x8B, 0xF2,
        0x48, 0x8B, 0x49, 0x70, 0x4C, 0x8B, 0xC2,
        0x8B, 0x47, 0x6C
    };
    return image && signature_size <= sizeof(expected) &&
        rva <= image_size && image_size - rva >= signature_size &&
        memcmp(image + rva, expected, signature_size) == 0;
}

uintptr_t AcMewSelectNativeHouseMoveRva(
    const uint8_t* image,
    size_t image_size) {
    const struct {
        uintptr_t rva;
        size_t signature_size;
    } candidates[] = {
        {AC_NATIVE_HOUSE_MOVE_STABLE_RVA, 15U},
        {AC_NATIVE_HOUSE_MOVE_BETA_RVA, 31U}
    };
    uintptr_t selected = 0U;
    size_t index;
    for (index = 0U; index < sizeof(candidates) / sizeof(candidates[0]);
         ++index) {
        if (!AcSignatureMatches(
                image,
                image_size,
                candidates[index].rva,
                candidates[index].signature_size)) {
            continue;
        }
        if (selected != 0U) {
            return 0U;
        }
        selected = candidates[index].rva;
    }
    return selected;
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
    void* room;
    if (!AcValidHouseCat(house_cat)) {
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
    uintptr_t native_move_rva;
    memset(&result, 0, sizeof(result));
    executable = GetModuleHandleW(NULL);
    if (!executable) {
        return result;
    }
    native_move_rva = AcMewSelectNativeHouseMoveRva(
        (const uint8_t*)executable,
        AcExecutableImageSize(executable));
    if (native_move_rva == 0U) {
        return result;
    }
    native_move = (uint8_t*)executable + native_move_rva;
    result.signature_valid = 1U;
    result.cat_valid = (uint8_t)AcValidHouseCat(house_cat);
    result.target_room_valid =
        (uint8_t)AcReadable(target_room, sizeof(void*));
    if (!result.cat_valid || !result.target_room_valid) {
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
