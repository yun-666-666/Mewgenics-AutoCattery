#include "auto_cattery/recommendation/mapping_probe.hpp"

namespace autocattery::recommendation {
namespace {

constexpr std::uint32_t kStableSamplesRequired = 3;

}  // namespace

void MappingProbeSession::Arm() noexcept {
    armed_ = true;
    complete_ = false;
    summary_ = {};
}

void MappingProbeSession::Clear() noexcept {
    armed_ = false;
    complete_ = false;
    summary_ = {};
}

bool MappingProbeSession::ShouldSample(
    const ui::UiContextSnapshot& context) const noexcept {
    return armed_ && !complete_ &&
           context.kind == ui::UiContextKind::EmbarkSelection &&
           context.input_enabled && !context.save_in_progress &&
           context.scene_generation != 0;
}

void MappingProbeSession::Observe(
    const ui::UiContextSnapshot& context,
    const AnonymousMappingObservation& observation) noexcept {
    if (!ShouldSample(context)) {
        if (armed_ && context.kind == ui::UiContextKind::UnsafeTransition) {
            Clear();
        }
        return;
    }

    if (summary_.scene_generation != context.scene_generation ||
        summary_.observation != observation) {
        summary_ = {
            .scene_generation = context.scene_generation,
            .observation = observation,
            .stable_samples = 1
        };
        return;
    }

    ++summary_.stable_samples;
    if (summary_.stable_samples >= kStableSamplesRequired) {
        summary_.stable_component_roles =
            observation.role_count != 0;
        // No current local source proves either identity or visual boundary.
        summary_.stable_cat_id_boundary = false;
        summary_.stable_view_identity_boundary = false;
        summary_.visual_marker_boundary = false;
        complete_ = true;
    }
}

bool MappingProbeSession::Armed() const noexcept {
    return armed_;
}

bool MappingProbeSession::Complete() const noexcept {
    return complete_;
}

const MappingProbeSummary& MappingProbeSession::Summary() const noexcept {
    return summary_;
}

}  // namespace autocattery::recommendation
