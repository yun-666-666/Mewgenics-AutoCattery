#pragma once

#include <filesystem>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/execution/backup_service.hpp"

namespace autocattery::save_safety {

struct BackupListing {
    std::string operation_id;
    bool manifest_valid{};
    std::string detail;
};

class BackupCatalog final {
public:
    explicit BackupCatalog(std::filesystem::path backup_root);

    [[nodiscard]] Result<execution::BackupArtifact> Find(
        std::string_view operation_id) const;
    [[nodiscard]] std::vector<BackupListing> List() const;

private:
    std::filesystem::path backup_root_;
};

}  // namespace autocattery::save_safety
