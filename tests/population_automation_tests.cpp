#include "../src/ui/in_game_panel_controller.hpp"
#include "../src/ui/mew_ui_house_cat_probe.h"
#include "../src/ui/mew_ui_house_detail_adapter.h"
#include "../src/ui/mew_ui_delivery_trace.h"
#include "test_support.hpp"

#include <algorithm>
#include <fstream>

namespace {
using namespace autocattery;
std::vector<int64_t> cats, delivered;
AcDeliveryTrace trace{};
ui::ManagementPanelContent shown;
bool attached{};
int scene;
}

// Native calls and rendering are the boundary: exercise the real controller,
// save selection, population ranking, countdown, and sequential delivery.
extern "C" size_t AcMewCountHouseCats(void*) { return cats.size(); }
extern "C" AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void* manager, const int64_t* ids, size_t count, AcMewHouseCatMatch* matches, size_t capacity) {
    AC_CHECK(manager == &scene);
    AcMewHouseCatIdentityProbe result{};
    result.house_cat_count = static_cast<uint32_t>(cats.size());
    result.stable_bijection = result.consistent_mapping = 1;
    for (size_t i = 0; i < count && i < capacity; ++i)
        if (std::ranges::find(cats, ids[i]) != cats.end())
            matches[result.match_count++] = {ids[i], &scene, nullptr};
    return result;
}
extern "C" AcDeliveryTrace AcMewReadDeliveryTrace(void*) { return trace; }
extern "C" AcMewHouseDetailResult AcMewOpenHouseCatDetails(void*, void*) { return {}; }
extern "C" int AcMewOpenDeadCatPipe(void*, void*, int64_t) { return 0; }
extern "C" int AcMewChooseDeadCatRecipient(void*, int64_t) { return 0; }
extern "C" int AcMewOpenPopulationCatPipe(void*, void*, int64_t id) {
    trace.cat_id = id;
    trace.layout_valid = trace.drawer_count = trace.cat_mode = trace.selected_valid = 1;
    return 1;
}
extern "C" int AcMewChooseTrashRecipient(void*, int64_t id) {
    AC_CHECK(trace.cat_id == id);
    delivered.push_back(id);
    std::erase(cats, id);
    trace.selected_valid = 0;
    return 1;
}
extern "C" int AcMewFindPopulationRecipient(void*, int64_t) { return 7; }
extern "C" int AcMewChoosePopulationRecipient(void* scene, int64_t id, int recipient) {
    AC_CHECK(recipient == 7);
    return AcMewChooseTrashRecipient(scene, id);
}

namespace autocattery::ui {
MewUiManagementPanelView::~MewUiManagementPanelView() = default;
Result<void> MewUiManagementPanelView::Attach(const UiContextSnapshot&) { attached = true; return {}; }
void MewUiManagementPanelView::Detach() noexcept { attached = false; }
void MewUiManagementPanelView::AbandonScene() noexcept { attached = false; }
Result<void> MewUiManagementPanelView::Show(const ManagementPanelContent& content) { shown = content; return {}; }
void MewUiManagementPanelView::Hide() noexcept {}
std::optional<ManagementPanelEvent> MewUiManagementPanelView::Poll() { return {}; }
bool MewUiManagementPanelView::IsAttached() const noexcept { return attached; }
bool MewUiManagementPanelView::IsVisible() const noexcept { return false; }
const char* MewUiManagementPanelView::ResolveMode() const noexcept { return "test"; }
std::uint64_t MewUiManagementPanelView::LastAttachElapsedUs() const noexcept { return 0; }
std::uint64_t MewUiManagementPanelView::LastRenderElapsedUs() const noexcept { return 0; }
std::uint32_t MewUiManagementPanelView::LastChangedTextCount() const noexcept { return 0; }
std::uint32_t MewUiManagementPanelView::LastChangedFrameCount() const noexcept { return 0; }
void MewUiManagementPanelView::BeginNumericInput(std::size_t, std::string) {}
void MewUiManagementPanelView::CancelNumericInput() noexcept {}
bool MewUiManagementPanelView::IsEditing() const noexcept { return false; }

struct InGamePanelControllerTestAccess {
    static void Load(InGamePanelController& panel, const snapshot::HouseSnapshot& house,
                     const std::filesystem::path& root, bool duplicate = false) {
        if (panel.protection_task_.valid()) panel.protection_task_.wait();
        std::vector<snapshot::HouseSnapshot> saves{house};
        if (duplicate) saves.push_back(house);
        auto model = std::make_shared<protection::ProtectionEditorModel>(
            root / "absent-sidecar.json", std::move(saves));
        AC_CHECK(static_cast<bool>(model->Reload()));
        const_cast<protection::ProtectionSaveOption&>(model->saves()[0]).path = root / "fixture.sav";
        std::promise<InGamePanelController::ProtectionLoad> promise;
        promise.set_value({model});
        panel.protection_task_ = promise.get_future();
        panel.protection_loading_ = true;
    }
    static void Expire(InGamePanelController& panel) {
        panel.population_deadline_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    }
    static void AllowCheck(InGamePanelController& panel) { panel.population_next_check_ = {}; }
    static bool Loading(const InGamePanelController& panel) { return panel.population_auto_loading_; }
    static bool Preview(const InGamePanelController& panel) { return panel.population_auto_preview_; }
    static bool Delivering(const InGamePanelController& panel) { return panel.delivery_.Active(); }
    static bool Resolved(const InGamePanelController& panel) { return panel.current_save_.has_value(); }
    static void Resolve(InGamePanelController& panel) { panel.PollProtectionLoad(); }
    static void Close(InGamePanelController& panel) { panel.Close(); }
};
}

int main() {
    using namespace autocattery;
    using Access = ui::InGamePanelControllerTestAccess;
    const auto root = std::filesystem::temp_directory_path() /
        ("autocattery-population-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root / "config");
    std::filesystem::copy_file(std::filesystem::path(__FILE__).parent_path().parent_path() /
        "config/default_config.json", root / "config/default_config.json",
        std::filesystem::copy_options::overwrite_existing);
    const auto write_config = [&](bool read_only) {
        std::ofstream(root / "config/user_config.json") <<
            "{\"general\":{\"safe_mode\":false},\"execution_safety\":{\"read_only_mode\":" <<
            (read_only ? "true" : "false") << "},\"room_planning\":{\"population_limit\":150}}";
    };
    write_config(false);
    snapshot::HouseSnapshot house;
    house.capabilities.stable_cat_id = house.capabilities.read_genetic_stats = true;
    house.capabilities.read_sexuality = house.capabilities.read_relationships = true;
    for (int id = 1; id <= 173; ++id) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        cat.display_name = std::to_string(id);
        cat.life_stage = id == 1 ? snapshot::LifeStage::Senior : snapshot::LifeStage::Adult;
        cat.genetic_stats.values.fill(id < 30 ? 2 : 7);
        cat.available_for_combat = snapshot::TriState::Yes;
        cat.available_for_breeding = snapshot::TriState::No;
        cat.in_adventure_box = id == 2;
        cat.age_days = 20;
        house.cats.push_back(cat);
        cats.push_back(id);
    }
    ui::MewUiManagementPanelView view;
    workflow::OrganizeWorkflowFacade workflow;
    {
        ui::InGamePanelController panel(view, workflow, root, root);
        ui::UiContextSnapshot context{ui::UiContextKind::House, "House", 1, true, false};
        panel.Poll(context, &scene, false, false);
        AC_CHECK(Access::Loading(panel));
        Access::Load(panel, house, root);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(Access::Resolved(panel) && Access::Preview(panel));
        AC_CHECK(shown.title.find("10") != std::string::npos && delivered.empty());
        panel.Poll(context, &scene, false, true); // Esc cancels this visit.
        Access::AllowCheck(panel);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(!Access::Loading(panel) && !Access::Preview(panel) && delivered.empty());

        ++context.scene_generation;
        Access::AllowCheck(panel);
        panel.Poll(context, &scene, false, false);
        panel.Poll(context, &scene, false, false); // Restore scene pointer after detach.
        AC_CHECK(Access::Loading(panel));
        Access::Load(panel, house, root);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(Access::Preview(panel));
        write_config(true);
        Access::Expire(panel);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(!Access::Delivering(panel) && delivered.empty());
        Access::Close(panel);

        write_config(false);
        ++context.scene_generation;
        Access::AllowCheck(panel);
        panel.Poll(context, &scene, false, false);
        panel.Poll(context, &scene, false, false);
        Access::Load(panel, house, root);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(Access::Preview(panel));
        Access::Expire(panel);
        panel.Poll(context, &scene, false, false);
        AC_CHECK(Access::Delivering(panel) && panel.IsOpen());
        for (int i = 0; i < 100 && Access::Delivering(panel); ++i)
            panel.Poll(context, &scene, false, false);
        AC_CHECK(!Access::Delivering(panel));
        AC_CHECK(cats.size() == 150 && delivered.size() == 23);
        AC_CHECK(std::ranges::find(cats, 2) != cats.end()); // Adventure-box protection.

        // Refresh cannot select an ambiguous or stale save, and a null scene
        // produces a useful reason instead of the unrelated protection hint.
        cats.clear();
        for (const auto& cat : house.cats) cats.push_back(cat.id);
        panel.Poll(context, &scene, false, false);
        Access::Load(panel, house, root, true);
        Access::Resolve(panel);
        AC_CHECK(!Access::Resolved(panel));
        cats.pop_back();
        Access::Load(panel, house, root);
        Access::Resolve(panel);
        AC_CHECK(!Access::Resolved(panel));
    }
    std::filesystem::remove_all(root);
    return tests::failures ? 1 : 0;
}
