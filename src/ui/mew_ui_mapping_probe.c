#include "mew_ui_mapping_probe.h"

#include <string.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

static uint64_t AcHashText(uint64_t value, const char* text, size_t size) {
    const unsigned char* current;
    const unsigned char* end;
    current = (const unsigned char*)text;
    end = current + size;
    for (; current < end; ++current) {
        value = (value ^ *current) * 1099511628211ULL;
    }
    return (value ^ 0xFFU) * 1099511628211ULL;
}

static int AcGetTypeName(void* component, MewNarrowString* output) {
    MewComponent* typed;
    if (!component || !output) {
        return 0;
    }
    typed = (MewComponent*)component;
    __try {
        if (!typed->vtable || !typed->vtable->GetObjectTypeSTR) {
            return 0;
        }
        memset(output, 0, sizeof(*output));
        typed->vtable->GetObjectTypeSTR(component, output);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int AcTypeEquals(
    const MewNarrowString* type_name,
    const char* literal) {
    const char* data;
    size_t size;
    size_t literal_size;
    data = MewUI_GetNarrowStringData(type_name);
    size = MewUI_GetNarrowStringSize(type_name);
    literal_size = strlen(literal);
    return data && size == literal_size &&
           memcmp(data, literal, literal_size) == 0;
}

AcMewAnonymousMappingObservation AcMewInspectAnonymousMapping(
    void* scene_manager) {
    AcMewAnonymousMappingObservation result;
    MewPodVectorPtr* components;
    uint32_t index;
    memset(&result, 0, sizeof(result));
    result.type_digest = 14695981039346656037ULL;
    result.role_digest = 14695981039346656037ULL;
    if (!scene_manager) {
        return result;
    }

    __try {
        components =
            *(MewPodVectorPtr**)((uint8_t*)scene_manager +
                                 MEW_OFF_SCENE_COMPONENT_LISTS);
        if (!components || !components->data || components->size > 4096U) {
            return result;
        }
        result.component_count = components->size;
        for (index = 0; index < components->size; ++index) {
            void* component;
            MewNarrowString type_name;
            const char* type_data;
            size_t type_size;
            char role[256];
            component = components->data[index];
            if (!AcGetTypeName(component, &type_name)) {
                continue;
            }
            ++result.typed_component_count;
            type_data = MewUI_GetNarrowStringData(&type_name);
            type_size = MewUI_GetNarrowStringSize(&type_name);
            if (type_data && type_size > 0U && type_size < 256U) {
                ++result.type_name_count;
                result.type_digest =
                    AcHashText(result.type_digest, type_data, type_size);
            }
            if (!AcTypeEquals(&type_name, "Button")) {
                continue;
            }
            ++result.button_count;
            if (MewUI_GetButtonRoleName(component, role, sizeof(role))) {
                ++result.role_count;
                result.role_digest =
                    AcHashText(result.role_digest, role, strlen(role));
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return result;
    }
    return result;
}
