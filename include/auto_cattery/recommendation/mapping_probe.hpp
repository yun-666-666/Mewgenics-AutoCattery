#pragma once

#include <cstdint>

#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::recommendation {

struct AnonymousMappingObservation {
    std::uint32_t component_count{};
    std::uint32_t typed_component_count{};
    std::uint32_t type_name_count{};
    std::uint32_t button_count{};
    std::uint32_t role_count{};
    std::uint64_t type_digest{};
    std::uint64_t role_digest{};

    bool operator==(const AnonymousMappingObservation&) const = default;
};

struct MappingProbeSummary {
    std::uint64_t scene_generation{};
    AnonymousMappingObservation observation;
    std::uint32_t stable_samples{};
    bool stable_component_roles{};
    bool stable_cat_id_boundary{};
    bool stable_view_identity_boundary{};
    bool visual_marker_boundary{};
};

class MappingProbeSession final {
public:
    void Arm() noexcept;
    void Clear() noexcept;
    [[nodiscard]] bool ShouldSample(
        const ui::UiContextSnapshot& context) const noexcept;
    void Observe(
        const ui::UiContextSnapshot& context,
        const AnonymousMappingObservation& observation) noexcept;

    [[nodiscard]] bool Armed() const noexcept;
    [[nodiscard]] bool Complete() const noexcept;
    [[nodiscard]] const MappingProbeSummary& Summary() const noexcept;

private:
    bool armed_{};
    bool complete_{};
    MappingProbeSummary summary_;
};

}  // namespace autocattery::recommendation
