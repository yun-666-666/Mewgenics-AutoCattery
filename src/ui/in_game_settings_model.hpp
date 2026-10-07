#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "auto_cattery/config.hpp"
#include "auto_cattery/settings_file_editor.hpp"

namespace autocattery::ui {

class InGameSettingsModel final {
public:
    InGameSettingsModel(
        std::filesystem::path default_config,
        std::filesystem::path user_config,
        std::filesystem::path data_mod_root = {});
    [[nodiscard]] Result<void> Reload();
    [[nodiscard]] Result<void> Adjust(
        std::size_t page, std::size_t row, int direction);
    [[nodiscard]] Result<void> AdjustFlat(
        std::size_t index, int direction);
    [[nodiscard]] std::optional<std::string> DirectValue(
        std::size_t index);
    [[nodiscard]] Result<void> SetFlatValue(
        std::size_t index, std::string_view text);
    [[nodiscard]] std::size_t PageCount();
    [[nodiscard]] std::string PageTitle(std::size_t page);
    [[nodiscard]] std::vector<std::string> Rows(std::size_t page);
    [[nodiscard]] std::vector<std::string> AllRows();
    [[nodiscard]] std::vector<std::string> GroupTitles() const;
    [[nodiscard]] bool IsEnglish() const noexcept;
    [[nodiscard]] const Config& CurrentConfig() const noexcept { return config_; }
    [[nodiscard]] bool RequiresGameRestart(std::size_t index) const noexcept;

private:
    struct Field {
        const char* label;
        std::variant<bool*, std::size_t*, double*, std::string*> value;
        double minimum{};
        double maximum{};
        double step{1.0};
    };
    struct Page {
        const char* group;
        std::vector<Field> fields;
    };

    [[nodiscard]] std::vector<Page> Pages();
    [[nodiscard]] std::optional<Field> FlatField(std::size_t index);
    [[nodiscard]] std::string Format(const Field& field) const;

    SettingsFileEditor editor_;
    std::filesystem::path data_mod_root_;
    Config config_;
    bool loaded_{};
};

}  // namespace autocattery::ui
