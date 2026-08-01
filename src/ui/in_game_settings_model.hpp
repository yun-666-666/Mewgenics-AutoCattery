#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

#include "auto_cattery/config.hpp"
#include "auto_cattery/settings_file_editor.hpp"

namespace autocattery::ui {

class InGameSettingsModel final {
public:
    InGameSettingsModel(
        std::filesystem::path default_config,
        std::filesystem::path user_config);
    [[nodiscard]] Result<void> Reload();
    [[nodiscard]] Result<void> Adjust(
        std::size_t page, std::size_t row, int direction);
    [[nodiscard]] std::size_t PageCount();
    [[nodiscard]] std::string PageTitle(std::size_t page);
    [[nodiscard]] std::vector<std::string> Rows(std::size_t page);

private:
    struct Field {
        const char* label;
        std::variant<bool*, std::size_t*, double*> value;
        double minimum{};
        double maximum{};
        double step{1.0};
    };
    struct Page {
        const char* group;
        std::vector<Field> fields;
    };

    [[nodiscard]] std::vector<Page> Pages();
    [[nodiscard]] std::string Format(const Field& field) const;

    SettingsFileEditor editor_;
    Config config_;
    bool loaded_{};
};

}  // namespace autocattery::ui
