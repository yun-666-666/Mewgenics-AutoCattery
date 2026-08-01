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
    void* house_scene_manager,
    bool f10_pressed,
    bool escape_pressed) {
    const bool house_ready = context.kind == UiContextKind::House &&
        context.input_enabled && !context.save_in_progress;
    if (!house_ready) {
        Detach();
        return;
    }
    house_scene_manager_ = house_scene_manager;
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
        if (view_.IsEditing()) {
            view_.CancelNumericInput();
            editing_setting_.reset();
            editing_text_.clear();
            status_ = "已取消数字输入";
            Render();
            return;
        }
        Close();
        return;
    }
    PollProtectionLoad();
    if (const auto event = view_.Poll()) Handle(*event);
}

void InGamePanelController::Open(const UiContextSnapshot& context) {
    (void)context;
    const auto loaded = settings_.Reload();
    status_ = loaded ? "左右两侧微调；点击数值中间可直接输入"
                     : "设置读取失败：" + loaded.message;
    protection_page_ = false;
    if (!protection_loading_) protection_.reset();
    open_ = true;
    cat_page_ = 0;
    choice_page_ = 0;
    protection_choice_ = ProtectionChoice::None;
    editing_setting_.reset();
    editing_text_.clear();
    selected_cat_.reset();
    current_save_.reset();
    current_save_checked_ = false;
    selected_room_.reset();
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18002",
        "F10 opened the in-game management panel.");
    Render();
}

void InGamePanelController::Close() noexcept {
    view_.Hide();
    open_ = false;
    editing_setting_.reset();
    editing_text_.clear();
    selected_cat_.reset();
    current_save_.reset();
    current_save_checked_ = false;
    house_scene_manager_ = nullptr;
    protection_choice_ = ProtectionChoice::None;
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18003",
        "The in-game management panel was closed and remains hidden.");
}

void InGamePanelController::Detach() noexcept {
    if (view_.IsAttached()) view_.Detach();
    open_ = false;
    attached_generation_ = 0;
    editing_setting_.reset();
    editing_text_.clear();
    selected_cat_.reset();
    current_save_.reset();
    current_save_checked_ = false;
    house_scene_manager_ = nullptr;
    protection_choice_ = ProtectionChoice::None;
    last_attach_error_.clear();
}

bool InGamePanelController::IsOpen() const noexcept { return open_; }

void InGamePanelController::Handle(const ManagementPanelEvent& event) {
    switch (event.control) {
    case ManagementPanelControl::Close:
        Close();
        return;
    case ManagementPanelControl::SettingsTab:
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
        protection_choice_ = ProtectionChoice::None;
        choice_page_ = 0;
        protection_page_ = false;
        status_ = "左右两侧微调；点击数值中间可直接输入";
        break;
    case ManagementPanelControl::ProtectionTab:
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
        protection_choice_ = ProtectionChoice::None;
        choice_page_ = 0;
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
        std::size_t count{};
        if (protection_) {
            if (protection_choice_ == ProtectionChoice::Level) count = 4;
            else if (protection_choice_ == ProtectionChoice::Room)
                count = protection_->rooms().size() + 1;
            else count = protection_->cats().size();
        }
        const auto pages = std::max<std::size_t>(1, (count + 8) / 9);
        auto& page = protection_choice_ == ProtectionChoice::None
            ? cat_page_ : choice_page_;
        page = step > 0 ? (page + 1) % pages
                        : (page == 0 ? pages - 1 : page - 1);
        if (protection_choice_ == ProtectionChoice::None)
            selected_cat_.reset();
        break;
    }
    case ManagementPanelControl::Row:
        if (protection_page_) HandleProtectionRow(event.row, event.direction);
        else HandleSettingsEvent(event);
        break;
    case ManagementPanelControl::BeginEdit:
    case ManagementPanelControl::EditChanged:
    case ManagementPanelControl::CommitEdit:
        if (!protection_page_) HandleSettingsEvent(event);
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
    if (editing_setting_ && *editing_setting_ < content.rows.size()) {
        content.rows[*editing_setting_] = "<  输入：" + editing_text_ + "  >";
    }
    content.selected_row = editing_setting_;
    return content;
}

}  // namespace autocattery::ui
