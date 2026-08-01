#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::snapshot::detail {

struct MutationCatalogEntry {
    VisualTraitKind kind{VisualTraitKind::Mutation};
};

using MutationCatalog =
    std::unordered_map<std::string, MutationCatalogEntry>;

void ParseVisualPartSlots(
    std::span<const std::uint8_t> bytes,
    std::size_t equipment_block_start,
    CatSnapshot& cat);

bool LoadMutationCatalog(
    const std::filesystem::path& gpak_path,
    MutationCatalog& catalog,
    std::string& error);

void ApplyMutationCatalog(
    CatSnapshot& cat,
    const MutationCatalog& catalog);

}  // namespace autocattery::snapshot::detail
