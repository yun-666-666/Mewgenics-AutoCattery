#pragma once

#include <chrono>
#include <functional>
#include <string_view>

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

    virtual ~RecommendationMarkerView() = default;
    virtual Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) = 0;
    virtual void Detach() noexcept = 0;
    virtual void SetStatus(RecommendationUiStatus status) = 0;
    virtual Result<void> ShowSummary(std::string_view summary) = 0;
    virtual void ClearSummary() noexcept = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
};

class RecommendationMarkerController {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using RequestHandler = std::function<void(std::uint64_t)>;

    explicit RecommendationMarkerController(
        RecommendationMarkerView& view,
        Clock clock = {});

    void ObserveRuntime(
        bool house_ready,
        bool interstitial_ready,
        bool expedition_ready);
    Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    void HandleClick();
    void CompleteProbe(std::uint64_t scene_generation);
    Result<void> ShowRecommendations(
        std::uint64_t scene_generation,
        std::string_view summary);
    void Poll();
    void SetRequestHandler(RequestHandler handler);

    [[nodiscard]] bool ShouldShow() const noexcept;
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] bool MarkerVisible() const noexcept;

private:
    RecommendationMarkerView& view_;
    Clock clock_;
    bool available_this_day_{true};
    bool next_day_pending_{};
    bool marker_visible_{};
    std::uint64_t attached_generation_{};
    RequestHandler request_handler_;
    std::chrono::steady_clock::time_point last_click_{};
    std::chrono::steady_clock::time_point ready_after_{};
    RecommendationUiStatus status_after_hold_{
        RecommendationUiStatus::Ready};
};

}  // namespace autocattery::ui
