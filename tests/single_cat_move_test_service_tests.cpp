#include "auto_cattery/save_safety/single_cat_move_test_service.hpp"

#include <chrono>
#include <fstream>

#include "auto_cattery/save_safety/backup_catalog.hpp"
#include "auto_cattery/save_safety/restore_service.hpp"
#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"
#include "auto_cattery/snapshot/house_state_writer.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::string Hex(std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const auto byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

void CreateSave(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> state) {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    snapshot::detail::sqlite3* database = nullptr;
    AC_CHECK(api.open_v2(path.string().c_str(), &database,
        0x00000002 | 0x00000004, nullptr) == 0);
    const auto sql =
        "CREATE TABLE properties (key TEXT, data INTEGER);"
        "INSERT INTO properties VALUES('current_day',32);"
        "CREATE TABLE cats (key INTEGER, data BLOB);"
        "CREATE TABLE files (key TEXT, data BLOB);"
        "INSERT INTO files VALUES('house_state',X'" + Hex(state) + "');";
    AC_CHECK(api.exec(database, sql.c_str(), nullptr, nullptr, nullptr) == 0);
    AC_CHECK(api.close_v2(database) == 0);
}

struct PassingBuild final : save_safety::IGameBuildGate {
    Result<std::string> Verify(const std::filesystem::path&) const override {
        return {"synthetic-current-build"};
    }
};

struct FakeProcesses final : save_safety::IGameProcessProbe {
    bool running{};
    Result<bool> IsRunning(const std::filesystem::path&) override {
        return {running};
    }
};

struct DeniedReplace final : save_safety::IAtomicFileReplacer {
    Result<void> ReplaceTemporary(
        const std::filesystem::path&,
        const std::filesystem::path&) override {
        return {ErrorCode::WriteConflict, "injected replacement failure"};
    }
};

struct CorruptOnceReplace final : save_safety::IAtomicFileReplacer {
    int calls{};
    save_safety::AtomicFileReplacer actual;
    Result<void> ReplaceTemporary(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination) override {
        const auto result = actual.ReplaceTemporary(temporary, destination);
        if (result && ++calls == 1) {
            std::ofstream(destination, std::ios::binary | std::ios::trunc)
                << "bad-readback";
        }
        return result;
    }
};

save_safety::SingleCatMoveTestRequest Request(
    const std::filesystem::path& root,
    const std::filesystem::path& save,
    const std::filesystem::path& game,
    std::string operation) {
    return {
        .test_root = root,
        .test_save = save,
        .game_executable = game,
        .operation_id = std::move(operation),
        .moved_index = 0,
        .placement_source_index = 1,
        .development_test_enabled = true,
        .stable_window = std::chrono::milliseconds(1)
    };
}

}  // namespace

void RunSingleCatMoveTestServiceTests() {
    if (!snapshot::detail::WinSqliteApi::Instance().Available()) {
        return;
    }
    const auto base = std::filesystem::temp_directory_path() /
        ("auto-cattery-single-move-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto game_root = base / "game";
    const auto root = base / "isolated";
    std::filesystem::create_directories(game_root);
    std::filesystem::create_directories(root);
    const auto game = game_root / "Mewgenics.exe";
    std::ofstream(game) << "synthetic game";
    const auto original = snapshot::SerializeHouseState(
        std::vector<snapshot::HouseStateEntry>{
            {101, "Floor1_Large", 1, 2, 3},
            {102, "Attic", 4, 5, 6}
        });
    AC_CHECK(static_cast<bool>(original));
    const auto save = root / "copy.sav";
    CreateSave(save, original.value);

    PassingBuild build;
    FakeProcesses processes;
    save_safety::AtomicFileReplacer replacer;
    save_safety::TestCopyHouseStateStore store;
    save_safety::SingleCatMoveTestService service(
        build, processes, replacer, store);
    const auto moved = service.MoveOne(Request(root, save, game, "move-one"));
    AC_CHECK(static_cast<bool>(moved));
    AC_CHECK(moved.value.independent_readback_verified);
    AC_CHECK(moved.value.original_room == "Floor1_Large");
    AC_CHECK(moved.value.target_room == "Attic");
    const auto moved_state = snapshot::ParseHouseState(store.Read(save).value);
    AC_CHECK(moved_state.value[0].room_id == "Attic");

    execution::BackupService backups(root / "backups");
    save_safety::BackupCatalog catalog(root / "backups");
    save_safety::RestoreService restore(catalog, backups, processes, replacer);
    const auto undone = restore.Restore({
        "move-one", "move-one-undo", save, game,
        std::chrono::milliseconds(1)
    });
    AC_CHECK(static_cast<bool>(undone));
    AC_CHECK(store.Read(save).value == original.value);

    auto disabled = Request(root, save, game, "disabled");
    disabled.development_test_enabled = false;
    AC_CHECK(!service.MoveOne(disabled));
    processes.running = true;
    AC_CHECK(!service.MoveOne(Request(root, save, game, "running")));
    processes.running = false;

    DeniedReplace denied;
    save_safety::SingleCatMoveTestService denied_service(
        build, processes, denied, store);
    AC_CHECK(!denied_service.MoveOne(
        Request(root, save, game, "replace-denied")));
    AC_CHECK(store.Read(save).value == original.value);

    CorruptOnceReplace corrupt;
    save_safety::SingleCatMoveTestService corrupt_service(
        build, processes, corrupt, store);
    const auto mismatch = corrupt_service.MoveOne(
        Request(root, save, game, "readback-mismatch"));
    AC_CHECK(!mismatch);
    AC_CHECK(mismatch.message.find("backup was restored") != std::string::npos);
    AC_CHECK(store.Read(save).value == original.value);
    std::filesystem::remove_all(base);
}

}  // namespace autocattery::tests
