#pragma once

#include <chrono>
#include <functional>
#include <future>
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
    virtual void SetState(
        OrganizeButtonState state,
        std::string_view detail) = 0;
    virtual void ShowPlaceholder() = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
};

class HouseButtonController {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    HouseButtonController(
        HouseButtonView& view,
        workflow::OrganizeWorkflowFacade& workflow,
        Clock clock = {});

    Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    void SetState(
        OrganizeButtonState state,
        std::string_view detail = {});
    [[nodiscard]] bool IsAttached() const noexcept;
    void HandleClick();
    void Poll();

private:
    HouseButtonView& view_;
    workflow::OrganizeWorkflowFacade& workflow_;
    Clock clock_;
    OrganizeButtonState state_{OrganizeButtonState::Hidden};
    std::uint64_t scene_generation_{};
    std::chrono::steady_clock::time_point last_click_{};
    std::future<Result<void>> preview_task_;
    std::uint64_t preview_generation_{};
    std::chrono::steady_clock::time_point ready_after_{};
};

}  // namespace autocattery::ui
