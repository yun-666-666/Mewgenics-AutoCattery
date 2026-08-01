#include "in_game_panel_controller.hpp"

#include <algorithm>
#include <chrono>

#include "auto_cattery/logger.hpp"

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
    if (view_.IsAttached() &&
        context.scene_generation != attached_generation_) {
        Detach();
    }
    if (!view_.IsAttached()) {
        const auto attached = view_.Attach(context);
        if (!attached) {
            status_ = attached.message;
            if (last_attach_error_ != attached.message) {
                last_attach_error_ = attached.message;
                Logger::Instance().Write(
                    LogLevel::Warn, "ManagementPanel", "AC18001",
                    "House panel attach deferred: " + attached.message);
            }
            return;
        }
        last_attach_error_.clear();
        attached_generation_ = context.scene_generation;
        Logger::Instance().Write(
            LogLevel::Info, "ManagementPanel", "AC18000",
            "House panel nodes attached and held on the hidden frame.");
    }
    if (f10_pressed) {
        if (open_) Close();
        else Open(context);
    }
    if (!open_) return;
    if (escape_pressed) {
        Close();
        return;
    }
    PollProtectionLoad();
    if (const auto event = view_.Poll()) Handle(*event);
}

void InGamePanelController::Open(const UiContextSnapshot& context) {
    (void)context;
    const auto loaded = settings_.Reload();
    status_ = loaded ? "左侧点击减少，右侧点击增加；修改会立即保存"
                     : "设置读取失败：" + loaded.message;
    protection_page_ = false;
    if (!protection_loading_) protection_.reset();
    open_ = true;
    cat_page_ = 0;
    selected_cat_.reset();
    selected_room_.reset();
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18002",
        "F10 opened the in-game management panel.");
    Render();
}

void InGamePanelController::Close() noexcept {
    view_.Hide();
    open_ = false;
    selected_cat_.reset();
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18003",
        "The in-game management panel was closed and remains hidden.");
}

void InGamePanelController::Detach() noexcept {
    if (view_.IsAttached()) view_.Detach();
    open_ = false;
    attached_generation_ = 0;
    selected_cat_.reset();
    last_attach_error_.clear();
}

bool InGamePanelController::IsOpen() const noexcept { return open_; }

void InGamePanelController::Handle(const ManagementPanelEvent& event) {
    switch (event.control) {
    case ManagementPanelControl::Close:
        Close();
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
    case ManagementPanelControl::Next:
    case ManagementPanelControl::Scroll: {
        if (!protection_page_) break;
        const int step = event.control == ManagementPanelControl::Scroll
            ? event.direction
            : (event.control == ManagementPanelControl::Next ? 1 : -1);
        const auto count = protection_ ? protection_->cats().size() : 0;
        const auto pages = std::max<std::size_t>(1, (count + 8) / 9);
        cat_page_ = step > 0 ? (cat_page_ + 1) % pages
                             : (cat_page_ == 0 ? pages - 1 : cat_page_ - 1);
        selected_cat_.reset();
        break;
    }
    case ManagementPanelControl::Row:
        if (protection_page_) HandleProtectionRow(event.row, event.direction);
        else {
            const auto adjusted = settings_.AdjustFlat(
                event.row, event.direction);
            status_ = adjusted ? "已保存；游戏中的规则会自动热更新"
                               : "保存失败：" + adjusted.message;
        }
        break;
    case ManagementPanelControl::Apply:
    case ManagementPanelControl::Remove:
        HandleProtectionRow(
            event.control == ManagementPanelControl::Apply ? 12 : 13, 0);
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
    ManagementPanelContent content;
    content.title = "设置";
    content.status = status_;
    content.rows = settings_.AllRows();
    return content;
}

}  // namespace autocattery::ui
