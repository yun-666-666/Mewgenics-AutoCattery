#include "auto_cattery/recommendation/mapping_probe.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

ui::UiContextSnapshot Context(
    ui::UiContextKind kind,
    std::uint64_t generation = 4) {
    return {kind, "fixture", generation, true, false, {"fixture"}};
}

}  // namespace

void RunRecommendationMappingProbeTests() {
    recommendation::MappingProbeSession session;
    const recommendation::AnonymousMappingObservation observation{
        .component_count = 81,
        .typed_component_count = 70,
        .button_count = 12,
        .role_count = 8,
        .role_digest = 1234
    };

    AC_CHECK(!session.ShouldSample(
        Context(ui::UiContextKind::EmbarkSelection)));
    session.Arm();
    AC_CHECK(session.Armed());
    AC_CHECK(!session.ShouldSample(Context(ui::UiContextKind::House)));
    AC_CHECK(session.ShouldSample(
        Context(ui::UiContextKind::EmbarkSelection)));

    session.Observe(
        Context(ui::UiContextKind::EmbarkSelection), observation);
    session.Observe(
        Context(ui::UiContextKind::EmbarkSelection), observation);
    AC_CHECK(!session.Complete());
    session.Observe(
        Context(ui::UiContextKind::EmbarkSelection), observation);
    AC_CHECK(session.Complete());
    AC_CHECK(session.Summary().stable_component_roles);
    AC_CHECK(!session.Summary().stable_cat_id_boundary);
    AC_CHECK(!session.Summary().stable_view_identity_boundary);
    AC_CHECK(!session.Summary().visual_marker_boundary);

    session.Arm();
    session.Observe(
        Context(ui::UiContextKind::EmbarkSelection), observation);
    auto changed = observation;
    ++changed.component_count;
    session.Observe(
        Context(ui::UiContextKind::EmbarkSelection), changed);
    AC_CHECK(session.Summary().stable_samples == 1);

    session.Observe(
        Context(ui::UiContextKind::UnsafeTransition), changed);
    AC_CHECK(!session.Armed());
    AC_CHECK(!session.Complete());
}

}  // namespace autocattery::tests
