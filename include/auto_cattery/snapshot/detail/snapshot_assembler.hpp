#pragma once

#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace autocattery::snapshot::detail {

Result<HouseSnapshot> AssembleHouseSnapshot(
    std::uint64_t snapshot_id,
    std::uint64_t scene_generation,
    std::optional<std::int64_t> current_day,
    std::string source_save_name,
    std::vector<CatSnapshot> cats,
    std::span<const HouseStateEntry> house_entries);

}  // namespace autocattery::snapshot::detail
