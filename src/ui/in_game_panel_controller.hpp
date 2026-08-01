#pragma once

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "auto_cattery/protection/editor_model.hpp"
#include "in_game_settings_model.hpp"
#include "mew_ui_management_panel_view.hpp"

namespace autocattery::ui {

class InGamePanelController final {
public:
    InGamePanelController(
        MewUiManagementPanelView& view,
        std::filesystem::path mod_root,
        std::filesystem::path game_root);
    ~InGamePanelController();
    void Poll(
        const UiContextSnapshot& context,
        bool f10_pressed,
        bool escape_pressed);
    void Detach() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    enum class ProtectionChoice { None, Level, Room };
    using ProtectionModel = protection::ProtectionEditorModel;
    using ProtectionLoad = Result<std::shared_ptr<ProtectionModel>>;

    void Open(const UiContextSnapshot& context);
    void Close() noexcept;
    void Handle(const ManagementPanelEvent& event);
    void HandleSettingsEvent(const ManagementPanelEvent& event);
    void HandleProtectionRow(std::size_t row, int direction);
    void StartProtectionLoad();
    void PollProtectionLoad();
    void Render();
    [[nodiscard]] ManagementPanelContent SettingsContent();
    [[nodiscard]] ManagementPanelContent ProtectionContent();

    MewUiManagementPanelView& view_;
    std::filesystem::path mod_root_;
    std::filesystem::path game_root_;
    InGameSettingsModel settings_;
    std::shared_ptr<ProtectionModel> protection_;
    std::future<ProtectionLoad> protection_task_;
    bool protection_loading_{};
    bool protection_page_{};
    bool open_{};
    std::uint64_t attached_generation_{};
    std::size_t cat_page_{};
    std::size_t choice_page_{};
    ProtectionChoice protection_choice_{ProtectionChoice::None};
    std::optional<std::size_t> editing_setting_;
    std::string editing_text_;
    std::optional<std::size_t> selected_cat_;
    protection::ProtectionLevel selected_level_{
        protection::ProtectionLevel::NoMove};
    std::optional<snapshot::RoomId> selected_room_;
    std::string status_;
    std::string last_attach_error_;
};

}  // namespace autocattery::ui
