#include "auto_cattery/ui/recommendation_marker_controller.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kClickDebounce = std::chrono::milliseconds(250);
constexpr auto kStatusHold = std::chrono::seconds(2);

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
    bool expedition_ready,
    bool save_selection_ready) {
    if (save_selection_ready) {
        if (!save_selection_active_) {
            available_this_day_ = true;
            next_day_pending_ = false;
            Detach();
            Logger::Instance().Write(
                LogLevel::Info,
                "RecommendationMarker",
                "AC4105",
                "Save selection observed; recommendation availability reset for the selected save.");
        }
        save_selection_active_ = true;
        return;
    }
    save_selection_active_ = false;

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

    if (house_ready && view_.IsAttached()) {
        SyncAvailability(available_this_day_ && !suppressed_);
    }

    if (!house_ready) {
        Detach();
    }
}

Result<void> RecommendationMarkerController::Attach(
    const UiContextSnapshot& context) {
    if (context.kind != UiContextKind::House ||
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

    auto result = view_.Attach(
        context,
        [this] {
            HandleClick();
        },
        [this](std::size_t index) {
            if (!suppressed_ &&
                marker_visible_ &&
                index < item_count_ &&
                details_handler_) {
                details_handler_(attached_generation_, index);
            }
        });
    if (!result) {
        return result;
    }

    marker_visible_ = false;
    request_pending_ = false;
    item_count_ = 0;
    attached_generation_ = context.scene_generation;
    ready_after_ = {};
    status_after_hold_ = RecommendationUiStatus::Ready;
    applied_availability_.reset();
    view_.ClearSummary();
    view_.SetStatus(RecommendationUiStatus::Ready);
    SyncAvailability(available_this_day_ && !suppressed_);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4100",
        "Attached AutoCattery.Recommendation.MarkCombatCatsButton.");
    return {};
}

void RecommendationMarkerController::Detach() noexcept {
    marker_visible_ = false;
    request_pending_ = false;
    item_count_ = 0;
    attached_generation_ = 0;
    last_click_ = {};
    ready_after_ = {};
    status_after_hold_ = RecommendationUiStatus::Ready;
    applied_availability_.reset();
    if (!view_.IsAttached()) {
        return;
    }
    view_.ClearSummary();
    view_.SetStatus(RecommendationUiStatus::Ready);
    view_.Detach();
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4101",
        "Cleared demo marker before detaching the recommendation control.");
}

void RecommendationMarkerController::AbandonScene() noexcept {
    marker_visible_ = false;
    request_pending_ = false;
    item_count_ = 0;
    attached_generation_ = 0;
    last_click_ = {};
    ready_after_ = {};
    status_after_hold_ = RecommendationUiStatus::Ready;
    applied_availability_.reset();
    suppressed_ = false;
    view_.AbandonScene();
}

void RecommendationMarkerController::SetSuppressed(bool suppressed) {
    if (suppressed_ == suppressed) return;
    suppressed_ = suppressed;
    if (suppressed_) {
        marker_visible_ = false;
        item_count_ = 0;
    }
    if (view_.IsAttached()) {
        SyncAvailability(available_this_day_ && !suppressed_);
    }
}

void RecommendationMarkerController::SetFurnitureMode(
    bool furniture_mode) {
    if (furniture_mode_ == furniture_mode) return;
    furniture_mode_ = furniture_mode;
    marker_visible_ = false;
    item_count_ = 0;
    request_pending_ = false;
    ready_after_ = {};
    applied_availability_.reset();
    view_.SetFurnitureMode(furniture_mode_);
    if (view_.IsAttached()) {
        view_.ClearSummary();
        SyncAvailability(available_this_day_ && !suppressed_);
    }
}

void RecommendationMarkerController::SyncAvailability(bool available) {
    if (applied_availability_.has_value() &&
        *applied_availability_ == available) {
        return;
    }
    view_.SetAvailable(available);
    applied_availability_ = available;
}

void RecommendationMarkerController::HandleClick() {
    if (!view_.IsAttached() ||
        suppressed_ ||
        furniture_mode_ ||
        !available_this_day_ ||
        request_pending_ ||
        ready_after_.time_since_epoch().count() != 0) {
        return;
    }

    const auto now = clock_();
    if (last_click_.time_since_epoch().count() != 0 &&
        now - last_click_ < kClickDebounce) {
        return;
    }
    last_click_ = now;

    if (marker_visible_) {
        marker_visible_ = false;
        item_count_ = 0;
        status_after_hold_ = RecommendationUiStatus::Ready;
        view_.ClearSummary();
        view_.SetStatus(RecommendationUiStatus::Ready);
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC12107",
            "Player cleared the read-only House recommendation items; "
            "expedition_selection_changed=0.");
        return;
    }

    marker_visible_ = false;
    request_pending_ = true;
    if (request_handler_) {
        request_handler_(attached_generation_);
    }
    view_.SetStatus(RecommendationUiStatus::ProbeRequired);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC12100",
        "Recommendation request entered read-only validation; no House "
        "cat details were opened.");
}

void RecommendationMarkerController::CompleteProbe(
    std::uint64_t scene_generation) {
    if (!view_.IsAttached() ||
        scene_generation != attached_generation_) {
        return;
    }
    request_pending_ = false;
    status_after_hold_ = RecommendationUiStatus::Ready;
    ready_after_ = clock_() + kStatusHold;
}

Result<void> RecommendationMarkerController::ShowRecommendations(
    std::uint64_t scene_generation,
    const std::vector<std::string>& labels) {
    if (!view_.IsAttached() ||
        suppressed_ ||
        scene_generation != attached_generation_) {
        return {
            ErrorCode::SceneUnavailable,
            "recommendation result belongs to a stale House scene"
        };
    }
    request_pending_ = false;

    if (labels.empty()) {
        return {
            ErrorCode::CatDataUnavailable,
            "recommendation result has no display items"
        };
    }
    const auto shown = view_.ShowItems(labels);
    if (!shown) {
        return shown;
    }
    marker_visible_ = true;
    item_count_ = labels.size();
    status_after_hold_ = RecommendationUiStatus::Marked;
    ready_after_ = clock_() + kStatusHold;
    return {};
}

void RecommendationMarkerController::Poll() {
    view_.Poll();
    if (furniture_mode_ && view_.IsAttached()) {
        // The native furniture UI advances copied House sign artwork
        // to its Clean Up frame. Keep the MOD control on its empty
        // stop frame without disabling the live Button component.
        view_.SetFurnitureMode(true);
    }
    if (ready_after_.time_since_epoch().count() == 0 ||
        clock_() < ready_after_) {
        return;
    }
    ready_after_ = {};
    if (view_.IsAttached()) {
        view_.SetStatus(status_after_hold_);
    }
}

void RecommendationMarkerController::SetRequestHandler(
    RequestHandler handler) {
    request_handler_ = std::move(handler);
}

void RecommendationMarkerController::SetDetailsHandler(
    DetailsHandler handler) {
    details_handler_ = std::move(handler);
}

bool RecommendationMarkerController::ShouldShow() const noexcept {
    return available_this_day_ && !suppressed_ && !furniture_mode_;
}

bool RecommendationMarkerController::IsAttached() const noexcept {
    return view_.IsAttached();
}

bool RecommendationMarkerController::IsSuppressed() const noexcept {
    return suppressed_;
}

bool RecommendationMarkerController::MarkerVisible() const noexcept {
    return marker_visible_;
}

}  // namespace autocattery::ui
