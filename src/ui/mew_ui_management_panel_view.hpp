#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "auto_cattery/error.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

enum class ManagementPanelControl {
    SettingsTab,
    ProtectionTab,
    Close,
    Previous,
    Next,
    Apply,
    Remove,
    Row
};

struct ManagementPanelEvent {
    ManagementPanelControl control{ManagementPanelControl::Close};
    std::size_t row{};
    int direction{};
};

struct ManagementPanelContent {
    std::string title;
    std::string status;
    std::vector<std::string> rows;
    bool protection_page{};
    bool show_apply{};
    bool show_remove{};
    std::optional<std::size_t> selected_row;
};

class MewUiManagementPanelView final {
public:
    ~MewUiManagementPanelView();
    Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    Result<void> Show(const ManagementPanelContent& content);
    void Hide() noexcept;
    [[nodiscard]] std::optional<ManagementPanelEvent> Poll();
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] bool IsVisible() const noexcept;

private:
    struct HitResult {
        ManagementPanelControl control;
        std::size_t row{};
        int direction{};
    };

    static LRESULT CALLBACK MessageHook(
        int code, WPARAM remove_message, LPARAM message_pointer);
    bool InstallHook() noexcept;
    void RemoveHook() noexcept;
    bool ResolveNodes() noexcept;
    bool CanTouchScene() const noexcept;
    [[nodiscard]] std::optional<HitResult> HitTest(HWND window) const noexcept;
    void SetElement(std::size_t index, const char* text, int frame) noexcept;

    void* scene_manager_{};
    std::uint64_t generation_{};
    void* background_{};
    std::array<void*, 15> elements_{};
    HHOOK message_hook_{};
    std::atomic_bool visible_{false};
    std::atomic<int> pending_control_{-1};
    std::atomic<int> pending_row_{-1};
    std::atomic<int> pending_direction_{};
};

}  // namespace autocattery::ui
