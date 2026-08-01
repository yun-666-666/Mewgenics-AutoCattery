#include "in_game_panel_controller.hpp"

#include <algorithm>
#include <chrono>

namespace autocattery::ui {

InGamePanelController::InGamePanelController(
    MewUiManagementPanelView& view,
    std::filesystem::path mod_root,
    std::filesystem::path game_root)
    : view_(view),
      mod_root_(std::move(mod_root)),
      game_root_(std::move(game_root)),
      settings_(
          mod_root_ / L"config" / L"default_config.json",
          mod_root_ / L"config" / L"user_config.json") {}

InGamePanelController::~InGamePanelController() { Detach(); }

void InGamePanelController::Poll(
    const UiContextSnapshot& context,
    bool f10_pressed,
    bool escape_pressed) {
    const bool house_ready = context.kind == UiContextKind::House &&
        context.input_enabled && !context.save_in_progress;
    if (!house_ready) {
        Detach();
        return;
    }
    if (open_ && context.scene_generation != generation_) {
        Detach();
    }
    if (f10_pressed) {
        if (open_) Detach();
        else Open(context);
    }
    if (!open_) return;
    if (escape_pressed) {
        Detach();
        return;
    }
    PollProtectionLoad();
    if (const auto event = view_.Poll()) Handle(*event);
}

void InGamePanelController::Open(const UiContextSnapshot& context) {
    if (!view_.IsAttached()) {
        const auto attached = view_.Attach(context);
        if (!attached) {
            status_ = attached.message;
            return;
        }
    }
    const auto loaded = settings_.Reload();
    status_ = loaded ? "左侧点击减少，右侧点击增加；修改会立即保存"
                     : "设置读取失败：" + loaded.message;
    protection_page_ = false;
    if (!protection_loading_) protection_.reset();
    open_ = true;
    generation_ = context.scene_generation;
    settings_page_ = 0;
    cat_page_ = 0;
    selected_cat_.reset();
    selected_room_.reset();
    Render();
}

void InGamePanelController::Detach() noexcept {
    if (view_.IsAttached()) view_.Detach();
    open_ = false;
    generation_ = 0;
    selected_cat_.reset();
}

bool InGamePanelController::IsOpen() const noexcept { return open_; }

void InGamePanelController::Handle(const ManagementPanelEvent& event) {
    switch (event.control) {
    case ManagementPanelControl::Close:
        Detach();
        return;
    case ManagementPanelControl::SettingsTab:
        protection_page_ = false;
        status_ = "左侧点击减少，右侧点击增加；修改会立即保存";
        break;
    case ManagementPanelControl::ProtectionTab:
        protection_page_ = true;
        if (!protection_ && !protection_loading_) StartProtectionLoad();
        break;
    case ManagementPanelControl::Previous:
    case ManagementPanelControl::Next: {
        const int step = event.control == ManagementPanelControl::Next ? 1 : -1;
        if (protection_page_) {
            const auto count = protection_ ? protection_->cats().size() : 0;
            const auto pages = std::max<std::size_t>(1, (count + 4) / 5);
            cat_page_ = step > 0 ? (cat_page_ + 1) % pages
                                 : (cat_page_ == 0 ? pages - 1 : cat_page_ - 1);
            selected_cat_.reset();
        } else {
            const auto pages = std::max<std::size_t>(1, settings_.PageCount());
            settings_page_ = step > 0 ? (settings_page_ + 1) % pages
                : (settings_page_ == 0 ? pages - 1 : settings_page_ - 1);
        }
        break;
    }
    case ManagementPanelControl::Row:
        if (protection_page_) HandleProtectionRow(event.row, event.direction);
        else {
            const auto adjusted = settings_.Adjust(
                settings_page_, event.row, event.direction);
            status_ = adjusted ? "已保存；游戏中的规则会自动热更新"
                               : "保存失败：" + adjusted.message;
        }
        break;
    case ManagementPanelControl::Apply:
    case ManagementPanelControl::Remove:
        HandleProtectionRow(
            event.control == ManagementPanelControl::Apply ? 8 : 9, 0);
        break;
    }
    if (open_) Render();
}

void InGamePanelController::Render() {
    const auto shown = view_.Show(
        protection_page_ ? ProtectionContent() : SettingsContent());
    if (!shown) Detach();
}

ManagementPanelContent InGamePanelController::SettingsContent() {
    return {settings_.PageTitle(settings_page_), status_,
            settings_.Rows(settings_page_), false, false, false, std::nullopt};
}

}  // namespace autocattery::ui
