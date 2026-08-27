#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

enum class RecommendationUiStatus {
    Ready,
    ProbeRequired,
    Marked
};

class RecommendationMarkerView {
public:
    using ClickHandler = std::function<void()>;
    using ItemClickHandler = std::function<void(std::size_t)>;

    virtual ~RecommendationMarkerView() = default;
    virtual Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler,
        ItemClickHandler item_click_handler) = 0;
    virtual void Detach() noexcept = 0;
    virtual void AbandonScene() noexcept = 0;
    virtual void SetAvailable(bool available) = 0;
    virtual void SetStatus(RecommendationUiStatus status) = 0;
    virtual Result<void> ShowItems(
        const std::vector<std::string>& labels) = 0;
    virtual void ClearSummary() noexcept = 0;
    virtual void Poll() = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
};

class RecommendationMarkerController {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using RequestHandler = std::function<void(std::uint64_t)>;
    using DetailsHandler =
        std::function<void(std::uint64_t, std::size_t)>;

    explicit RecommendationMarkerController(
        RecommendationMarkerView& view,
        Clock clock = {});

    void ObserveRuntime(
        bool house_ready,
        bool interstitial_ready,
        bool expedition_ready,
        bool save_selection_ready = false);
    Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    void AbandonScene() noexcept;
    void SetSuppressed(bool suppressed);
    void HandleClick();
    void CompleteProbe(std::uint64_t scene_generation);
    Result<void> ShowRecommendations(
        std::uint64_t scene_generation,
        const std::vector<std::string>& labels);
    void Poll();
    void SetRequestHandler(RequestHandler handler);
    void SetDetailsHandler(DetailsHandler handler);

    [[nodiscard]] bool ShouldShow() const noexcept;
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] bool IsAttachedToGeneration(
        std::uint64_t scene_generation) const noexcept;
    [[nodiscard]] bool IsSuppressed() const noexcept;
    [[nodiscard]] bool MarkerVisible() const noexcept;

private:
    void SyncAvailability(bool available);

    RecommendationMarkerView& view_;
    Clock clock_;
    bool available_this_day_{true};
    bool next_day_pending_{};
    bool save_selection_active_{};
    bool suppressed_{};
    bool marker_visible_{};
    bool request_pending_{};
    std::optional<bool> applied_availability_;
    std::uint64_t attached_generation_{};
    RequestHandler request_handler_;
    DetailsHandler details_handler_;
    std::size_t item_count_{};
    std::chrono::steady_clock::time_point last_click_{};
    std::chrono::steady_clock::time_point ready_after_{};
    RecommendationUiStatus status_after_hold_{
        RecommendationUiStatus::Ready};
};

}  // namespace autocattery::ui
