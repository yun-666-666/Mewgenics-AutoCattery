#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <string>
#include <string_view>

#include "auto_cattery/error.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::workflow {
class OrganizeWorkflowFacade;
}

namespace autocattery::ui {

enum class OrganizeButtonState {
    Hidden,
    DisabledUnsupportedBuild,
    DisabledBusy,
    Ready,
    Running,
    Completed,
    Failed
};

class HouseButtonView {
public:
    using ClickHandler = std::function<void()>;

    virtual ~HouseButtonView() = default;
    virtual Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) = 0;
    virtual void Detach() noexcept = 0;
    virtual void AbandonScene() noexcept = 0;
    virtual void SetState(
        OrganizeButtonState state,
        std::string_view detail) = 0;
    virtual void SetFurnitureMode(bool furniture_mode) = 0;
    virtual void ShowPlaceholder() = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
};

class HouseButtonController {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using BeforePreview = std::function<void()>;
    using FurnitureAction = std::function<void(std::uint64_t)>;

    HouseButtonController(
        HouseButtonView& view,
        workflow::OrganizeWorkflowFacade& workflow,
        Clock clock = {},
        BeforePreview before_preview = {});

    Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    void AbandonScene() noexcept;
    void SetSuppressed(bool suppressed);
    void SetFurnitureMode(bool furniture_mode);
    void SetFurnitureActionHandler(FurnitureAction handler);
    void SetFurnitureActionAvailable(bool available);
    void SetState(
        OrganizeButtonState state,
        std::string_view detail = {});
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] bool IsSuppressed() const noexcept;
    void HandleClick();
    void Poll();

private:
    void StartPreview(bool continuation);
    void ExecuteBatch();

    HouseButtonView& view_;
    workflow::OrganizeWorkflowFacade& workflow_;
    Clock clock_;
    BeforePreview before_preview_;
    OrganizeButtonState state_{OrganizeButtonState::Hidden};
    std::string state_detail_;
    bool suppressed_{};
    bool furniture_mode_{};
    bool furniture_action_available_{};
    FurnitureAction furniture_action_;
    std::uint64_t scene_generation_{};
    std::chrono::steady_clock::time_point last_click_{};
    std::future<Result<void>> preview_task_;
    std::uint64_t preview_generation_{};
    bool continuation_preview_pending_{};
    bool continuation_preview_{};
    bool continuation_execute_pending_{};
    bool awaiting_execution_{};
    std::chrono::steady_clock::time_point ready_after_{};
    std::size_t state_sync_poll_{};
    std::size_t next_state_sync_poll_{};
};

}  // namespace autocattery::ui
