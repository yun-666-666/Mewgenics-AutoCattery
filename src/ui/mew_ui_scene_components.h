#pragma once

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

#ifdef __cplusplus
extern "C" {
#endif

MewPodVectorPtr* AcMewGetValidatedSceneComponents(void* scene_manager);

#ifdef __cplusplus
}
#endif
