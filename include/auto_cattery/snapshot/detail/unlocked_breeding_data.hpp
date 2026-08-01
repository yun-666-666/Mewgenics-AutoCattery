#pragma once

#include <optional>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/detail/pedigree_parser.hpp"
#include "auto_cattery/snapshot/detail/progress_unlocks.hpp"

namespace autocattery::snapshot::detail {

class SaveDatabase;

struct UnlockedBreedingData {
    ProgressUnlocks unlocks;
    std::optional<PedigreeData> pedigree;
};

[[nodiscard]] Result<UnlockedBreedingData> LoadUnlockedBreedingData(
    const SaveDatabase& database);

}  // namespace autocattery::snapshot::detail
