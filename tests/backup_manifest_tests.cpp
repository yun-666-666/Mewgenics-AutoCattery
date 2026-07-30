#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/save_safety/backup_manifest_store.hpp"

#include <chrono>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

void Write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

}  // namespace

void RunBackupManifestTests() {
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-manifest-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto source = root / "fixture.sav";
    Write(source, "synthetic database copy");

    execution::BackupService service(root / "backups");
    const auto backup = service.CreateVerifiedBackup({
        "manifest-safe", source, true, "unknown", 12,
        std::chrono::milliseconds(1)
    });
    AC_CHECK(static_cast<bool>(backup));
    AC_CHECK(std::filesystem::exists(backup.value.manifest_file));
    const auto manifest = save_safety::ReadBackupManifest(
        backup.value.manifest_file);
    AC_CHECK(static_cast<bool>(manifest));
    AC_CHECK(manifest.value.operation_id == "manifest-safe");
    AC_CHECK(manifest.value.game_day == 12);
    AC_CHECK(manifest.value.original_save.sha256 == backup.value.content_hash);
    AC_CHECK(manifest.value.source_identity == backup.value.source_identity);
    AC_CHECK(manifest.value.source_identity.size() == 64);
    std::ifstream manifest_text(backup.value.manifest_file, std::ios::binary);
    const std::string serialized_manifest{
        std::istreambuf_iterator<char>(manifest_text), {}};
    AC_CHECK(serialized_manifest.find("fixture.sav") == std::string::npos);
    manifest_text.close();
    AC_CHECK(static_cast<bool>(service.VerifyBackup(backup.value)));

    Write(backup.value.backup_file, "corrupted");
    AC_CHECK(!service.VerifyBackup(backup.value));
    Write(backup.value.manifest_file, "not json");
    AC_CHECK(!save_safety::ReadBackupManifest(backup.value.manifest_file));
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
