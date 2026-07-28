#include "mew_ui_scene_probe.h"

#include <string.h>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

size_t AcMewEnumerateScenes(
    AcMewSceneRecord* records,
    size_t record_capacity) {
    MewDirector* mew_director;
    void** current;
    void** end;
    size_t count;

    if (!records || record_capacity == 0U) {
        return 0U;
    }

    mew_director = MewUI_GetMewDirector();
    if (!mew_director || !mew_director->director) {
        return 0U;
    }

    count = 0U;
    __try {
        current = mew_director->director->scenes.begin;
        end = mew_director->director->scenes.end;
        while (current && current < end && count < record_capacity) {
            void* scene_manager;
            MewNarrowString* scene_name;
            const char* scene_name_data;
            size_t scene_name_size;
            MewPodVectorPtr* components;
            AcMewSceneRecord* record;

            scene_manager = *current++;
            if (!scene_manager) {
                continue;
            }

            scene_name =
                (MewNarrowString*)((uint8_t*)scene_manager +
                                   MEW_OFF_SCENE_NAME);
            scene_name_data = MewUI_GetNarrowStringData(scene_name);
            scene_name_size = MewUI_GetNarrowStringSize(scene_name);
            if (!scene_name_data ||
                scene_name_size >= AC_MEW_SCENE_NAME_CAPACITY) {
                continue;
            }

            record = &records[count];
            memset(record, 0, sizeof(*record));
            record->scene_manager = scene_manager;
            memcpy(
                record->scene_name,
                scene_name_data,
                scene_name_size);
            record->scene_name[scene_name_size] = '\0';
            record->ready =
                (uint8_t)(MewUI_IsSceneReadyForUITick(scene_manager) != 0);

            components =
                *(MewPodVectorPtr**)((uint8_t*)scene_manager +
                                     MEW_OFF_SCENE_COMPONENT_LISTS);
            if (components) {
                record->component_count = components->size;
            }
            ++count;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return count;
    }

    return count;
}
