#include "in_game_panel_controller.hpp"

#include <algorithm>
#include <chrono>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {

InGamePanelController::InGamePanelController(
    MewUiManagementPanelView& view,
    workflow::OrganizeWorkflowFacade& workflow,
    std::filesystem::path mod_root,
    std::filesystem::path game_root)
    : view_(view),
      workflow_(workflow),
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
        AbandonScene();
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
            "House panel nodes attached and held on the hidden frame; mode=" +
                std::string(view_.ResolveMode()) +
                " attach_us=" +
                std::to_string(view_.LastAttachElapsedUs()));
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
            status_ = English() ? "Numeric input cancelled"
                                : "已取消数字输入";
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
    status_ = loaded
        ? (English()
            ? "Use the sides to adjust; click the center to type a value"
            : "左右两侧微调；点击数值中间可直接输入")
        : (English() ? "Settings load failed: " : "设置读取失败：") +
            loaded.message;
    page_ = ManagementPanelPage::Settings;
    if (!protection_loading_) protection_.reset();
    open_ = true;
    cat_page_ = 0;
    choice_page_ = 0;
    preview_page_ = 0;
    preview_.pages.clear();
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
        page_ = ManagementPanelPage::Settings;
        status_ = English()
            ? "Use the sides to adjust; click the center to type a value"
            : "左右两侧微调；点击数值中间可直接输入";
        break;
    case ManagementPanelControl::ProtectionTab:
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
        protection_choice_ = ProtectionChoice::None;
        choice_page_ = 0;
        page_ = ManagementPanelPage::Protection;
        if (!protection_ && !protection_loading_) StartProtectionLoad();
        break;
    case ManagementPanelControl::PreviewTab:
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
        protection_choice_ = ProtectionChoice::None;
        page_ = ManagementPanelPage::Preview;
        LoadPreview();
        break;
    case ManagementPanelControl::Previous:
    case ManagementPanelControl::Next:
    case ManagementPanelControl::Scroll: {
        if (page_ == ManagementPanelPage::Settings) break;
        const int step = event.control == ManagementPanelControl::Scroll
            ? event.direction
            : (event.control == ManagementPanelControl::Next ? 1 : -1);
        if (page_ == ManagementPanelPage::Preview) {
            const auto pages = std::max<std::size_t>(1, preview_.pages.size());
            preview_page_ = step > 0
                ? (preview_page_ + 1) % pages
                : (preview_page_ == 0 ? pages - 1 : preview_page_ - 1);
            break;
        }
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
        if (page_ == ManagementPanelPage::Protection)
            HandleProtectionRow(event.row, event.direction);
        else if (page_ == ManagementPanelPage::Settings)
            HandleSettingsEvent(event);
        break;
    case ManagementPanelControl::BeginEdit:
    case ManagementPanelControl::EditChanged:
    case ManagementPanelControl::CommitEdit:
        if (page_ == ManagementPanelPage::Settings) HandleSettingsEvent(event);
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
    auto content = page_ == ManagementPanelPage::Protection
        ? ProtectionContent()
        : (page_ == ManagementPanelPage::Preview
            ? PreviewContent() : SettingsContent());
    content.group_titles = settings_.GroupTitles();
    const auto shown = view_.Show(content);
    if (!shown) {
        Detach();
        return;
    }
    Logger::Instance().Write(
        LogLevel::Info, "ManagementPanel", "AC18004",
        "F10 panel render completed; elapsed_us=" +
            std::to_string(view_.LastRenderElapsedUs()) +
            " changed_text=" +
            std::to_string(view_.LastChangedTextCount()) +
            " changed_frames=" +
            std::to_string(view_.LastChangedFrameCount()));
}

void InGamePanelController::AbandonScene() noexcept {
    view_.AbandonScene();
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

ManagementPanelContent InGamePanelController::SettingsContent() {
    ManagementPanelContent content;
    content.page = ManagementPanelPage::Settings;
    content.title = English() ? "Settings" : "设置";
    content.status = status_;
    content.rows = settings_.AllRows();
    if (editing_setting_ && *editing_setting_ < content.rows.size()) {
        content.rows[*editing_setting_] =
            std::string("<  ") + (English() ? "Input: " : "输入：") +
            editing_text_ + "  >";
    }
    content.selected_row = editing_setting_;
    return content;
}

bool InGamePanelController::English() const noexcept {
    return settings_.IsEnglish();
}

void InGamePanelController::LoadPreview() {
    preview_page_ = 0;
    const auto latest = workflow_.LatestPreview();
    if (!latest) {
        preview_.pages.clear();
        status_ = English()
            ? "Create a plan with Auto-Organize first, then return here."
            : "请先点击“自动整理猫舍”生成计划，再回到这里查看。";
        return;
    }
    preview_ = BuildDetailedPreview(latest.value, English());
    status_ = English()
        ? "Loaded the latest unexpired preview; this page never moves cats."
        : "已读取最新且未过期的预览；此页面本身不会移动猫。";
}

ManagementPanelContent InGamePanelController::PreviewContent() {
    ManagementPanelContent content;
    content.page = ManagementPanelPage::Preview;
    if (preview_.pages.empty()) {
        content.title = English() ? "Full Preview" : "完整预览";
        content.status = status_;
        content.rows = {English()
            ? "No preview is available. Close F10, click Auto-Organize once, and return."
            : "暂无预览。请关闭 F10，点击一次“自动整理猫舍”后再回来。"};
        return content;
    }
    preview_page_ = std::min(preview_page_, preview_.pages.size() - 1);
    const auto& page = preview_.pages[preview_page_];
    content.title = page.title;
    content.status = page.status;
    content.rows = page.rows;
    content.show_navigation = preview_.pages.size() > 1;
    return content;
}

}  // namespace autocattery::ui
