#pragma once

#include <span>
#include "auto_cattery/config.hpp"
#include "auto_cattery/protection/editor_model.hpp"

namespace autocattery::breeding {
struct PopulationSelection {
    std::vector<snapshot::CatId> retained;
    std::vector<snapshot::CatId> surplus;
    std::size_t protected_count{};
    bool limit_reached{};
};

Result<PopulationSelection> SelectPopulation(
    const snapshot::HouseSnapshot& house,
    std::span<const protection::ProtectionCatOption> protection,
    const Config& config);
}
