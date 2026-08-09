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
    bool expedition_ready,
    bool save_selection_ready) {
    (void)interstitial_ready;
    (void)expedition_ready;
    if (save_selection_ready) {
        Detach();
        return;
    }
    if (house_ready && view_.IsAttached()) {
        SyncAvailability(!suppressed_);
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
    SyncAvailability(!suppressed_);
    Logger::Instance().Write(
        LogLevel::Info,
        "FurnitureAnalysis",
        "AC3200",
        "Attached AutoCattery.Furniture.StartAnalysisButton.");
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
        "FurnitureAnalysis",
        "AC3202",
        "Detached the furniture analysis control.");
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
    if (view_.IsAttached()) {
        SyncAvailability(!suppressed_);
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
        view_.ClearSummary();
    }

    marker_visible_ = false;
    request_pending_ = true;
    if (request_handler_) {
        request_handler_(attached_generation_);
    }
    view_.SetStatus(RecommendationUiStatus::ProbeRequired);
    Logger::Instance().Write(
        LogLevel::Info,
        "FurnitureAnalysis",
        "AC3203",
        "Player requested a read-only furniture analysis.");
}

void RecommendationMarkerController::CompleteProbe(
    std::uint64_t scene_generation) {
    if (!view_.IsAttached() ||
        scene_generation != attached_generation_) {
        return;
    }
    request_pending_ = false;
    ready_after_ = {};
    view_.SetStatus(RecommendationUiStatus::Ready);
}

Result<void> RecommendationMarkerController::ShowRecommendations(
    std::uint64_t scene_generation,
    const std::vector<std::string>& labels) {
    if (!view_.IsAttached() ||
        suppressed_ ||
        scene_generation != attached_generation_) {
        return {
            ErrorCode::SceneUnavailable,
            "furniture analysis belongs to a stale House scene"
        };
    }
    request_pending_ = false;

    if (labels.empty()) {
        return {
            ErrorCode::CatDataUnavailable,
            "furniture analysis has no display items"
        };
    }
    const auto shown = view_.ShowItems(labels);
    if (!shown) {
        return shown;
    }
    marker_visible_ = true;
    item_count_ = labels.size();
    status_after_hold_ = RecommendationUiStatus::Marked;
    ready_after_ = {};
    view_.SetStatus(RecommendationUiStatus::Marked);
    return {};
}

void RecommendationMarkerController::Poll() {
    view_.Poll();
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
    return !suppressed_;
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
