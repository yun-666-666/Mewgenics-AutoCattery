#include "auto_cattery/snapshot/detail/unlocked_breeding_data.hpp"

#include "auto_cattery/snapshot/detail/save_database.hpp"

namespace autocattery::snapshot::detail {

Result<UnlockedBreedingData> LoadUnlockedBreedingData(
    const SaveDatabase& database) {
    std::string error;
    std::optional<std::vector<std::byte>> progress;
    if (!database.ReadFileBlob("npc_progress", progress, error)) {
        return {
            {}, ErrorCode::CatDataUnavailable,
            "progress unlock query failed: " + error
        };
    }
    UnlockedBreedingData result;
    if (progress) {
        result.unlocks = ParseProgressUnlocks(*progress);
    }
    if (!result.unlocks.pedigree) {
        return {std::move(result)};
    }
    std::optional<std::vector<std::byte>> pedigree;
    if (!database.ReadFileBlob("pedigree", pedigree, error) || !pedigree) {
        return {
            {}, ErrorCode::CatDataUnavailable,
            "unlocked pedigree is unavailable: " + error
        };
    }
    auto parsed = ParsePedigreeBlob(*pedigree);
    if (!parsed) {
        return {
            {}, parsed.code,
            "unlocked pedigree parsing failed: " + parsed.message
        };
    }
    result.pedigree = std::move(parsed.value);
    return {std::move(result)};
}

}  // namespace autocattery::snapshot::detail
