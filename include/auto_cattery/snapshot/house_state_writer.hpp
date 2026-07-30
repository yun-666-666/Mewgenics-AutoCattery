#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

namespace autocattery::snapshot {

struct SingleCatTestRelocation {
    std::vector<std::uint8_t> encoded_house_state;
    HouseStateEntry original;
    HouseStateEntry relocated;
    std::size_t moved_index{};
    std::size_t placement_source_index{};
};

[[nodiscard]] Result<std::vector<std::uint8_t>> SerializeHouseState(
    std::span<const HouseStateEntry> entries);

[[nodiscard]] Result<SingleCatTestRelocation> BuildSingleCatTestRelocation(
    std::span<const std::uint8_t> original_blob,
    std::size_t moved_index,
    std::size_t placement_source_index);

}  // namespace autocattery::snapshot
