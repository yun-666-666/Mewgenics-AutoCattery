#include "auto_cattery/ui/recommendation_marker_controller.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kClickDebounce = std::chrono::milliseconds(250);

}  // namespace

RecommendationMarkerController::RecommendationMarkerController(
    RecommendationMarkerView& view,
    Clock clock)
    : view_(view),
      clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = [] {
            return std::chrono::steady_clock::now();
        };
    }
}

void RecommendationMarkerController::ObserveRuntime(
    bool house_ready,
    bool interstitial_ready,
    bool expedition_ready) {
    if (expedition_ready && available_this_day_) {
        available_this_day_ = false;
        next_day_pending_ = false;
        Detach();
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC4103",
            "Expedition scene observed; recommendation control is closed for this day.");
    }

    if (interstitial_ready && !available_this_day_) {
        next_day_pending_ = true;
    }

    if (house_ready && next_day_pending_) {
        available_this_day_ = true;
        next_day_pending_ = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC4104",
            "Next-day House observed; recommendation control is available again.");
    }

    if (!house_ready) {
        Detach();
    }
}

Result<void> RecommendationMarkerController::Attach(
    const UiContextSnapshot& context) {
    if (!available_this_day_ ||
        context.kind != UiContextKind::House ||
        !context.input_enabled ||
        context.save_in_progress) {
        return {
            ErrorCode::SceneUnavailable,
            "recommendation control is not available in this House state"
        };
    }
    if (view_.IsAttached()) {
        return {};
    }

    auto result = view_.Attach(context, [this] {
        HandleClick();
    });
    if (!result) {
        return result;
    }

    marker_visible_ = false;
    view_.SetMarkerVisible(false);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4100",
        "Attached AutoCattery.Recommendation.MarkCombatCatsButton.");
    return {};
}

void RecommendationMarkerController::Detach() noexcept {
    marker_visible_ = false;
    last_click_ = {};
    if (!view_.IsAttached()) {
        return;
    }
    view_.SetMarkerVisible(false);
    view_.Detach();
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4101",
        "Cleared demo marker before detaching the recommendation control.");
}

void RecommendationMarkerController::HandleClick() {
    if (!view_.IsAttached() || !available_this_day_) {
        return;
    }

    const auto now = clock_();
    if (last_click_.time_since_epoch().count() != 0 &&
        now - last_click_ < kClickDebounce) {
        return;
    }
    last_click_ = now;

    marker_visible_ = !marker_visible_;
    view_.SetMarkerVisible(marker_visible_);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4102",
        marker_visible_
            ? "Enabled MOD-owned demo marker; no cat or team state was read or changed."
            : "Cleared MOD-owned demo marker; no cat or team state was read or changed.");
}

bool RecommendationMarkerController::ShouldShow() const noexcept {
    return available_this_day_;
}

bool RecommendationMarkerController::IsAttached() const noexcept {
    return view_.IsAttached();
}

bool RecommendationMarkerController::MarkerVisible() const noexcept {
    return marker_visible_;
}

}  // namespace autocattery::ui
