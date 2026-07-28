#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

#include "auto_cattery/error.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

struct RecommendationVisual {
    void* target_handle{};
    void* outline_node{};
    void* badge_node{};
    void* score_text_node{};
    std::uint64_t scene_generation{};
};

enum class RecommendationButtonState {
    Hidden,
    Ready,
    Marking,
    Marked
};

class RecommendationMarkerView {
public:
    using ClickHandler = std::function<void()>;

    virtual ~RecommendationMarkerView() = default;
    virtual Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) = 0;
    virtual Result<RecommendationVisual> ShowDemoTextMarker(
        std::uint64_t scene_generation) = 0;
    virtual void ClearAllVisuals() noexcept = 0;
    virtual void Detach() noexcept = 0;
    virtual void SetButtonState(RecommendationButtonState state) = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
    [[nodiscard]] virtual void* SafeTestTarget() const noexcept = 0;
};

class RecommendationMarkerController {
public:
    explicit RecommendationMarkerController(RecommendationMarkerView& view);

    Result<void> AttachButton(const UiContextSnapshot& context);
    Result<void> ShowDemoMarker(void* safe_test_target);
    void ClearAll() noexcept;
    void Detach() noexcept;
    void HandleClick();

    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] bool HasMarkers() const noexcept;
    [[nodiscard]] std::uint64_t SceneGeneration() const noexcept;

private:
    RecommendationMarkerView& view_;
    RecommendationVisual demo_visual_{};
    RecommendationButtonState state_{RecommendationButtonState::Hidden};
    std::uint64_t scene_generation_{};
};

}  // namespace autocattery::ui
