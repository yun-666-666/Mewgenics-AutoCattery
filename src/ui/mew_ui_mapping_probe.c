#include "mew_ui_mapping_probe.h"
#include "mew_ui_scene_components.h"

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
static int AcCopyNarrowString(
    const MewNarrowString* value,
    char* output,
    size_t output_capacity) {
    const char* data;
    size_t size;
    data = MewUI_GetNarrowStringData(value);
    size = MewUI_GetNarrowStringSize(value);
    if (!data || !output || size == 0U || size >= output_capacity) {
        return 0;
    }
    memcpy(output, data, size);
    output[size] = '\0';
    return 1;
}
static int AcComponentHasRootNode(void* component) {
    void* root_node;
    if (!component) {
        return 0;
    }
    __try {
        root_node = *(void**)((uint8_t*)component + 0x38U);
        return root_node && root_node != component;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static MewPodVectorPtr* AcGetComponents(void* scene_manager) {
    if (!scene_manager) {
        return NULL;
    }
    return AcMewGetValidatedSceneComponents(scene_manager);
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
        components = AcMewGetValidatedSceneComponents(scene_manager);
        if (!components) {
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
size_t AcMewEnumerateMappingTypes(
    void* scene_manager,
    AcMewMappingTypeRecord* records,
    size_t record_capacity) {
    MewPodVectorPtr* components;
    uint32_t index;
    size_t record_count;
    if (!records || record_capacity == 0U) {
        return 0U;
    }
    memset(records, 0, record_capacity * sizeof(*records));
    components = AcGetComponents(scene_manager);
    if (!components) {
        return 0U;
    }
    record_count = 0U;
    for (index = 0U; index < components->size; ++index) {
        MewNarrowString type_name;
        char name[AC_MEW_MAPPING_NAME_CAPACITY];
        size_t record_index;
        void* component;
        component = components->data[index];
        if (!AcGetTypeName(component, &type_name) ||
            !AcCopyNarrowString(&type_name, name, sizeof(name))) {
            continue;
        }
        for (record_index = 0U;
             record_index < record_count;
             ++record_index) {
            if (strcmp(records[record_index].type_name, name) == 0) {
                break;
            }
        }
        if (record_index == record_count) {
            if (record_count >= record_capacity) {
                continue;
            }
            strcpy_s(
                records[record_count].type_name,
                sizeof(records[record_count].type_name),
                name);
            ++record_count;
        }
        ++records[record_index].component_count;
        if (AcComponentHasRootNode(component)) {
            ++records[record_index].root_node_count;
        }
    }
    return record_count;
}
size_t AcMewEnumerateButtonRoles(
    void* scene_manager,
    AcMewMappingRoleRecord* records,
    size_t record_capacity) {
    MewPodVectorPtr* components;
    uint32_t index;
    size_t record_count;
    if (!records || record_capacity == 0U) {
        return 0U;
    }
    memset(records, 0, record_capacity * sizeof(*records));
    components = AcGetComponents(scene_manager);
    if (!components) {
        return 0U;
    }
    record_count = 0U;
    for (index = 0U; index < components->size; ++index) {
        MewNarrowString type_name;
        char role[AC_MEW_MAPPING_NAME_CAPACITY];
        size_t record_index;
        void* component;
        component = components->data[index];
        if (!AcGetTypeName(component, &type_name) ||
            !AcTypeEquals(&type_name, "Button") ||
            !MewUI_GetButtonRoleName(component, role, sizeof(role))) {
            continue;
        }
        for (record_index = 0U;
             record_index < record_count;
             ++record_index) {
            if (strcmp(records[record_index].role_name, role) == 0) {
                break;
            }
        }
        if (record_index == record_count) {
            if (record_count >= record_capacity) {
                continue;
            }
            strcpy_s(
                records[record_count].role_name,
                sizeof(records[record_count].role_name),
                role);
            ++record_count;
        }
        ++records[record_index].button_count;
    }
    return record_count;
}
