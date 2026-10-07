#pragma once

#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "auto_cattery/protection/editor_model.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "in_game_preview_model.hpp"
#include "in_game_settings_model.hpp"
#include "dead_cat_delivery_service.hpp"
#include "auto_cattery/breeding/population_selection.hpp"
#include "mew_ui_management_panel_view.hpp"

namespace autocattery::ui {

class InGamePanelController final {
public:
    InGamePanelController(
        MewUiManagementPanelView& view,
        workflow::OrganizeWorkflowFacade& workflow,
        std::filesystem::path mod_root,
        std::filesystem::path game_root);
    ~InGamePanelController();
    void Poll(
        const UiContextSnapshot& context,
        void* house_scene_manager,
        bool f10_pressed,
        bool escape_pressed);
    void Detach() noexcept;
    void AbandonScene() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    friend struct InGamePanelControllerTestAccess;
    enum class ProtectionChoice { None, Level, Room };
    using ProtectionModel = protection::ProtectionEditorModel;
    using ProtectionLoad = Result<std::shared_ptr<ProtectionModel>>;

    void PollDeliveryTrace();
    void Open(const UiContextSnapshot& context);
    void Close() noexcept;
    void Handle(const ManagementPanelEvent& event);
    void HandleSettingsEvent(const ManagementPanelEvent& event);
    void HandleProtectionRow(std::size_t row, int direction);
    void StartProtectionLoad();
    void PollProtectionLoad();
    void PollPopulationAutomation(const UiContextSnapshot& context);
    void CancelPopulationAutomation() noexcept;
    void ResolveCurrentSave();
    void LoadPreview();
    void HandleSeniorRow(std::size_t row);
    void StartDeadCatDelivery();
    std::vector<const snapshot::CatSnapshot*> SeniorCats();
    ManagementPanelContent SeniorContent();
    void Render();
    [[nodiscard]] ManagementPanelContent SettingsContent();
    [[nodiscard]] ManagementPanelContent ProtectionContent();
    [[nodiscard]] ManagementPanelContent PreviewContent();
    [[nodiscard]] bool English() const noexcept;

    MewUiManagementPanelView& view_;
    workflow::OrganizeWorkflowFacade& workflow_;
    std::filesystem::path mod_root_;
    std::filesystem::path game_root_;
    InGameSettingsModel settings_;
    std::shared_ptr<ProtectionModel> protection_;
    std::future<ProtectionLoad> protection_task_;
    bool protection_loading_{};
    ManagementPanelPage page_{ManagementPanelPage::Settings};
    bool open_{};
    bool delivery_trace_enabled_{};
    unsigned delivery_trace_records_{};
    std::string last_delivery_trace_;
    std::uint64_t attached_generation_{};
    std::size_t cat_page_{};
    std::size_t choice_page_{};
    std::size_t preview_page_{};
    std::size_t senior_page_{};
    bool senior_sort_by_stats_{true};
    bool delivery_preview_{};
    bool population_preview_{};
    std::optional<Result<breeding::PopulationSelection>> population_plan_;
    std::string population_error_;
    bool population_auto_loading_{};
    bool population_auto_preview_{};
    std::uint64_t population_checked_generation_{};
    std::size_t population_checked_limit_{};
    std::chrono::steady_clock::time_point population_next_check_{};
    std::chrono::steady_clock::time_point population_deadline_{};
    int population_countdown_{-1};
    DeadCatDeliveryService delivery_;
    DetailedPreviewModel preview_;
    ProtectionChoice protection_choice_{ProtectionChoice::None};
    std::optional<std::size_t> editing_setting_;
    std::string editing_text_;
    std::optional<std::size_t> selected_cat_;
    std::optional<std::size_t> current_save_;
    bool current_save_checked_{};
    void* house_scene_manager_{};
    protection::ProtectionLevel selected_level_{
        protection::ProtectionLevel::NoMove};
    std::optional<snapshot::RoomId> selected_room_;
    std::string status_;
    std::string last_attach_error_;
    std::chrono::steady_clock::time_point next_attach_retry_{};
};

}  // namespace autocattery::ui
