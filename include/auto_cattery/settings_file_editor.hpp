#pragma once

#include <filesystem>

#include "auto_cattery/config.hpp"

namespace autocattery {

struct SettingsFilePaths {
    std::filesystem::path default_config;
    std::filesystem::path user_config;
};

class SettingsFileEditor final {
public:
    explicit SettingsFileEditor(SettingsFilePaths paths);

    [[nodiscard]] Result<Config> Load() const;
    [[nodiscard]] Result<Config> Save(const Config& config) const;

private:
    SettingsFilePaths paths_;
};

}  // namespace autocattery
