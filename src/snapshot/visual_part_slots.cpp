#include "auto_cattery/snapshot/detail/visual_traits.hpp"

#include <array>
#include <cstring>

namespace autocattery::snapshot::detail {
namespace {

struct SlotDefinition {
    const char* slot;
    const char* category;
    std::size_t table_index;
};

constexpr std::array<SlotDefinition, 15> kSlots{{
    {"fur", "texture", 0},
    {"body", "body", 3},
    {"head", "head", 8},
    {"tail", "tail", 13},
    {"leg_L", "legs", 18},
    {"leg_R", "legs", 23},
    {"arm_L", "legs", 28},
    {"arm_R", "legs", 33},
    {"eye_L", "eyes", 38},
    {"eye_R", "eyes", 43},
    {"eyebrow_L", "eyebrows", 48},
    {"eyebrow_R", "eyebrows", 53},
    {"ear_L", "ears", 58},
    {"ear_R", "ears", 63},
    {"mouth", "mouth", 68}
}};

constexpr std::size_t kVisualTableOffset = 68;

}  // namespace

void ParseVisualPartSlots(
    std::span<const std::uint8_t> bytes,
    std::size_t equipment_block_start,
    CatSnapshot& cat) {
    cat.raw_visual_part_slots.clear();
    cat.raw_visual_part_slots.reserve(kSlots.size());
    for (const auto& definition : kSlots) {
        const auto offset = equipment_block_start + kVisualTableOffset +
            definition.table_index * sizeof(std::uint32_t);
        if (offset > bytes.size() ||
            sizeof(std::uint32_t) > bytes.size() - offset) {
            cat.raw_visual_part_slots.clear();
            return;
        }
        std::uint32_t id{};
        std::memcpy(&id, bytes.data() + offset, sizeof(id));
        cat.raw_visual_part_slots.push_back({
            definition.slot,
            definition.category,
            id
        });
    }
}

}  // namespace autocattery::snapshot::detail
