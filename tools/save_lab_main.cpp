#include "auto_cattery/save_safety/atomic_file_replace.hpp"
#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "auto_cattery/save_safety/game_process_probe.hpp"
#include "auto_cattery/save_safety/single_cat_move_test_service.hpp"
#include "auto_cattery/save_safety/test_copy_guard.hpp"
#include "auto_cattery/save_safety/test_copy_house_state_store.hpp"
#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <windows.h>

#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

namespace {

struct Arguments {
    bool help{};
    bool list{};
    bool move_one{};
    bool enabled{};
    std::filesystem::path test_root;
    std::filesystem::path test_save;
    std::filesystem::path game_executable;
    std::filesystem::path sqlite_runtime;
    std::string operation_id;
    std::size_t cat_index{};
    std::size_t placement_source_index{};
};

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), text.data(), size, nullptr, nullptr);
    return text;
}

std::string OneLine(std::string value) {
    for (auto& byte : value) {
        if (static_cast<unsigned char>(byte) < 0x20U) {
            byte = '?';
        }
    }
    return value;
}

bool ParseIndex(std::wstring_view value, std::size_t& result) {
    const auto text = Utf8(std::wstring(value));
    std::size_t parsed{};
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || end != text.data() + text.size() ||
        parsed == 0) {
        return false;
    }
    result = parsed - 1;
    return true;
}

std::optional<Arguments> Parse(int argc, wchar_t** argv) {
    Arguments result;
    for (int index = 1; index < argc; ++index) {
        const std::wstring key(argv[index]);
        if (key == L"--help") {
            result.help = true;
            continue;
        }
        if (key == L"--list-placements") {
            result.list = true;
            continue;
        }
        if (key == L"--move-one") {
            result.move_one = true;
            continue;
        }
        if (key == L"--enable-current-build-test-copy-write") {
            result.enabled = true;
            continue;
        }
        if (index + 1 >= argc || key.rfind(L"--", 0) != 0) {
            return std::nullopt;
        }
        const std::wstring value(argv[++index]);
        if (key == L"--test-root") {
            result.test_root = value;
        } else if (key == L"--test-save") {
            result.test_save = value;
        } else if (key == L"--game-executable") {
            result.game_executable = value;
        } else if (key == L"--sqlite-runtime") {
            result.sqlite_runtime = value;
        } else if (key == L"--operation-id") {
            result.operation_id = Utf8(value);
        } else if (key == L"--cat-index") {
            if (!ParseIndex(value, result.cat_index)) {
                return std::nullopt;
            }
        } else if (key == L"--placement-source-index") {
            if (!ParseIndex(value, result.placement_source_index)) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    return result;
}

void Usage() {
    std::cout
        << "AutoCatterySaveLab --list-placements --test-root <dir> "
           "--test-save <copy.sav> --game-executable <Mewgenics.exe>\n"
        << "AutoCatterySaveLab --move-one --test-root <dir> "
           "--test-save <copy.sav> --game-executable <Mewgenics.exe> "
           "--operation-id <id> --cat-index <1-based> "
           "--placement-source-index <1-based> "
           "--sqlite-runtime <sqlite3.dll> "
           "--enable-current-build-test-copy-write\n";
}

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

int ListPlacements(
    const Arguments& arguments,
    const autocattery::save_safety::IGameBuildGate& build_gate,
    const autocattery::save_safety::TestCopyHouseStateStore& store) {
    const auto build = build_gate.Verify(arguments.game_executable);
    if (!build) {
        return Fail(build.message);
    }
    const auto save = autocattery::save_safety::ResolveIsolatedTestSave(
        arguments.test_root, arguments.test_save,
        arguments.game_executable);
    if (!save) {
        return Fail(save.message);
    }
    const auto blob = store.Read(save.value);
    if (!blob) {
        return Fail(blob.message);
    }
    const auto entries = autocattery::snapshot::ParseHouseState(blob.value);
    if (!entries) {
        return Fail(entries.message);
    }

    std::string error;
    auto database = autocattery::snapshot::detail::SaveDatabase::OpenReadOnly(
        save.value, error);
    std::optional<std::int32_t> day;
    std::vector<autocattery::snapshot::detail::CatStorageRecord> cats;
    if (!database || !database->ReadCurrentDay(day, error) ||
        !database->ReadCats(cats, error)) {
        return Fail("test-copy cat list read failed: " + error);
    }
    std::unordered_map<autocattery::snapshot::CatId, std::string> names;
    for (const auto& record : cats) {
        const auto parsed = autocattery::snapshot::ParseCatBlob(
            record.id,
            {reinterpret_cast<const std::uint8_t*>(record.blob.data()),
             record.blob.size()},
            day);
        if (parsed) {
            names.emplace(record.id, OneLine(parsed.value.display_name));
        }
    }
    std::cout << "build=" << build.value
              << " entries=" << entries.value.size() << '\n';
    for (std::size_t index = 0; index < entries.value.size(); ++index) {
        const auto& entry = entries.value[index];
        const auto found = names.find(entry.cat_id);
        std::cout << "index=" << index + 1
                  << " room=" << OneLine(entry.room_id)
                  << " name=" << (found == names.end() ? "<unavailable>" : found->second)
                  << '\n';
    }
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const auto arguments = Parse(argc, argv);
    if (!arguments) {
        Usage();
        return 1;
    }
    if (arguments->help) {
        Usage();
        return 0;
    }
    if (arguments->list == arguments->move_one ||
        arguments->test_root.empty() || arguments->test_save.empty() ||
        arguments->game_executable.empty()) {
        Usage();
        return 1;
    }
    auto build_gate = autocattery::save_safety::CurrentMewgenicsBuildGate();
    if (arguments->list) {
        autocattery::save_safety::TestCopyHouseStateStore store;
        return ListPlacements(*arguments, build_gate, store);
    }
    if (arguments->operation_id.empty() || !arguments->enabled ||
        arguments->sqlite_runtime.empty()) {
        return Fail(
            "move-one requires an operation ID, a SQLite runtime, and the explicit test-copy write switch");
    }
    constexpr int kStrictTablesMinimumSqliteVersion = 3037000;
    std::error_code path_error;
    const auto sqlite_path = std::filesystem::weakly_canonical(
        arguments->sqlite_runtime, path_error);
    const DWORD sqlite_attributes = path_error
        ? INVALID_FILE_ATTRIBUTES
        : GetFileAttributesW(sqlite_path.c_str());
    if (!arguments->sqlite_runtime.is_absolute() || path_error ||
        _wcsicmp(sqlite_path.filename().c_str(), L"sqlite3.dll") != 0 ||
        !std::filesystem::is_regular_file(sqlite_path, path_error) || path_error ||
        sqlite_attributes == INVALID_FILE_ATTRIBUTES ||
        (sqlite_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return Fail("SQLite runtime must be an absolute, non-reparse sqlite3.dll file");
    }
    const auto sqlite = autocattery::snapshot::detail::WinSqliteApi::LoadFrom(
        sqlite_path);
    if (!sqlite.Available() ||
        sqlite.VersionNumber() < kStrictTablesMinimumSqliteVersion) {
        return Fail("move-one requires SQLite 3.37.0 or newer for the current STRICT schema");
    }
    autocattery::save_safety::TestCopyHouseStateStore store(sqlite);
    autocattery::save_safety::WindowsGameProcessProbe processes;
    autocattery::save_safety::AtomicFileReplacer replacer;
    autocattery::save_safety::SingleCatMoveTestService service(
        build_gate, processes, replacer, store);
    const auto outcome = service.MoveOne({
        .test_root = arguments->test_root,
        .test_save = arguments->test_save,
        .game_executable = arguments->game_executable,
        .operation_id = arguments->operation_id,
        .moved_index = arguments->cat_index,
        .placement_source_index = arguments->placement_source_index,
        .development_test_enabled = true
    });
    if (!outcome) {
        return Fail(outcome.message);
    }
    std::cout << "operation=" << arguments->operation_id
              << " backup=verified"
              << " from_room=" << outcome.value.original_room
              << " to_room=" << outcome.value.target_room
              << " independent_readback=verified\n";
    return 0;
}
