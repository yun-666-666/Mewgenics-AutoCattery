#include "plan_sealer_snapshot.hpp"

#include <algorithm>
#include <tuple>

namespace autocattery::execution::detail {
namespace {

template<class T>
void Append(std::ostringstream& output, const T& value) {
    output << value << '|';
}

template<class T>
void AppendOptional(
    std::ostringstream& output,
    const std::optional<T>& value) {
    Append(output, value.has_value());
    if (value) {
        Append(output, *value);
    }
}

}  // namespace

void AppendSnapshotCapabilitiesAndBreeding(
    std::ostringstream& output,
    const snapshot::HouseSnapshot& snapshot) {
    const auto& c = snapshot.capabilities;
    Append(output, c.stable_cat_id);
    Append(output, c.read_room_assignments);
    Append(output, c.read_genetic_stats);
    Append(output, c.read_heredity_bonus);
    Append(output, c.read_equipment_bonus);
    Append(output, c.read_raw_ability_slots);
    Append(output, c.read_typed_abilities);
    Append(output, c.read_visual_traits);
    Append(output, c.read_class_id);
    Append(output, c.read_age);
    Append(output, c.read_sexuality);
    Append(output, c.read_breeding_eligibility);
    Append(output, c.read_relationships);
    Append(output, c.read_room_attributes);
    Append(output, c.read_room_capacities);

    auto cats = snapshot.cats;
    std::ranges::sort(cats, {}, &snapshot::CatSnapshot::id);
    for (const auto& cat : cats) {
        Append(output, cat.id);
        Append(output, static_cast<int>(cat.sexuality));
        AppendOptional(output, cat.sexuality_coefficient);
        AppendOptional(output, cat.parent_a_id);
        AppendOptional(output, cat.parent_b_id);
        AppendOptional(output, cat.inbreeding_coefficient);
    }

    auto pairs = snapshot.pedigree_pair_coefficients;
    std::ranges::sort(
        pairs,
        [](const auto& left, const auto& right) {
            return std::tie(left.cat_a_id, left.cat_b_id) <
                std::tie(right.cat_a_id, right.cat_b_id);
        });
    for (const auto& pair : pairs) {
        Append(output, pair.cat_a_id);
        Append(output, pair.cat_b_id);
        Append(output, pair.coefficient);
    }
}

}  // namespace autocattery::execution::detail
