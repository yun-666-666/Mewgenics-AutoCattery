#pragma once

#include "auto_cattery/config.hpp"

namespace autocattery::ui {
void InitializeBreedingAssistance(bool supported_build);
void ConfigureBreedingAssistance(const Config& config);
void DisableBreedingAssistance();
}
