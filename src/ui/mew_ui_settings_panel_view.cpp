#include "mew_ui_settings_panel_view.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "mew_ui_movie_clip.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 4> kRowNodes{
    "recommend_row_1",
    "recommend_row_2",
    "recommend_row_3",
    "recommend_row_4"
};
constexpr std::array<const char*, 4> kTextNodes{
    "recommend_text_1",
    "recommend_text_2",
    "recommend_text_3",
    "recommend_text_4"
};
constexpr double kVirtualWidth = 1280.0;
constexpr double kVirtualHeight = 720.0;

struct HitRectangle {
    double left;
    double top;
    double right;
    double bottom;
};

constexpr std::array<HitRectangle, 4> kHitRectangles{{
    {975.0, 170.0, 1110.0, 220.0},
    {1115.0, 170.0, 1250.0, 220.0},
    {975.0, 230.0, 1110.0, 280.0},
    {1115.0, 230.0, 1250.0, 280.0}
}};

std::string RowText(
    std::string_view title,
    const SettingsControl& control,
    bool first,
    bool selected) {
    std::string text;
    if (first && !title.empty()) {
        text.push_back(title.front());
        text.push_back(' ');
    }
    if (control.destructive_related) {
        text += "! ";
    }
    text += control.label;
    text.push_back(' ');
    text += control.value;
    if (control.read_only) {
        text += " fixed";
    }
    const auto status = title.find(' ');
    if (selected && status != std::string_view::npos) {
        text.push_back(' ');
        text.append(title.substr(status + 1));
    }
    return text;
}

}  // namespace

MewUiSettingsPanelView::~MewUiSettingsPanelView() {
    Detach();
}

Result<void> MewUiSettingsPanelView::Attach(
    const UiContextSnapshot& context) {
    auto* scene = MewUI_GetSceneByName(context.scene_name.c_str());
    if (context.kind != UiContextKind::House ||
        !context.input_enabled || context.save_in_progress ||
        scene == nullptr || MewUI_IsSceneReadyForUITick(scene) == 0 ||
        MewUI_IsSceneDestroying(scene) != 0) {
        return {
            ErrorCode::SceneUnavailable,
            "House is not ready for the settings panel"
        };
    }
    scene_manager_ = scene;
    attached_generation_ = context.scene_generation;
    for (std::size_t index = 0; index < row_nodes_.size(); ++index) {
        row_nodes_[index] = MewUI_FindNodeInSceneByName(
            scene_manager_,
            kRowNodes[index]);
        if (row_nodes_[index] == nullptr ||
            MewUI_FindNodeInSceneByName(
                scene_manager_,
                kTextNodes[index]) == nullptr) {
            Detach();
            return {
                ErrorCode::UiNodeNotFound,
                "a verified MOD settings row is unavailable"
            };
        }
    }
    active_ = true;
    ClearRows();
    return {};
}

void MewUiSettingsPanelView::Detach() noexcept {
    if (CanTouchScene()) {
        ClearRows();
    }
    active_ = false;
    scene_manager_ = nullptr;
    attached_generation_ = 0;
    row_nodes_.fill(nullptr);
}

Result<void> MewUiSettingsPanelView::Render(
    std::string_view title,
    std::span<const SettingsControl> controls,
    std::size_t selected_row) {
    if (!CanTouchScene() || controls.empty() || controls.size() > row_nodes_.size() ||
        selected_row >= controls.size()) {
        return {ErrorCode::SceneUnavailable, "settings rows cannot be rendered"};
    }
    for (std::size_t row = 0; row < row_nodes_.size(); ++row) {
        const bool visible = row < controls.size();
        const auto text = visible
            ? RowText(title, controls[row], row == 0, row == selected_row)
            : std::string{};
        if (MewUI_SetTextInSceneText(
                scene_manager_,
                kTextNodes[row],
                text.c_str()) == 0 ||
            !HoldMewUiMovieClipFrame(
                row_nodes_[row],
                visible ? (row == selected_row ? 2 : 1) : 0)) {
            return {ErrorCode::UiNodeNotFound, "settings row update failed"};
        }
    }
    return {};
}

bool MewUiSettingsPanelView::IsAttached() const noexcept {
    return active_ && CanTouchScene();
}

std::optional<SettingsPanelHit> MewUiSettingsPanelView::HitTest(
    HWND window) const noexcept {
    if (window == nullptr || !IsAttached()) {
        return std::nullopt;
    }
    POINT cursor{};
    RECT client{};
    if (GetCursorPos(&cursor) == 0 ||
        ScreenToClient(window, &cursor) == 0 ||
        GetClientRect(window, &client) == 0) {
        return std::nullopt;
    }
    const auto width = static_cast<double>(client.right - client.left);
    const auto height = static_cast<double>(client.bottom - client.top);
    if (width <= 0.0 || height <= 0.0) {
        return std::nullopt;
    }
    const auto x = static_cast<double>(cursor.x) * kVirtualWidth / width;
    const auto y = static_cast<double>(cursor.y) * kVirtualHeight / height;
    for (std::size_t row = 0; row < kHitRectangles.size(); ++row) {
        const auto& rectangle = kHitRectangles[row];
        if (x >= rectangle.left && x <= rectangle.right &&
            y >= rectangle.top && y <= rectangle.bottom) {
            const auto midpoint = (rectangle.left + rectangle.right) / 2.0;
            return SettingsPanelHit{row, x < midpoint ? -1 : 1};
        }
    }
    return std::nullopt;
}

bool MewUiSettingsPanelView::CanTouchScene() const noexcept {
    if (scene_manager_ == nullptr ||
        MewUI_GetSceneByName("House") != scene_manager_) {
        return false;
    }
    return MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
           MewUI_IsSceneDestroying(scene_manager_) == 0;
}

void MewUiSettingsPanelView::ClearRows() noexcept {
    if (scene_manager_ == nullptr) {
        return;
    }
    for (std::size_t row = 0; row < row_nodes_.size(); ++row) {
        MewUI_SetTextInSceneText(scene_manager_, kTextNodes[row], "");
        if (row_nodes_[row] != nullptr) {
            HoldMewUiMovieClipFrame(row_nodes_[row], 0);
        }
    }
}

}  // namespace autocattery::ui
