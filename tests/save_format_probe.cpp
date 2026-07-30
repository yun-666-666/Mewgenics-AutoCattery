#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace {

bool HasRawCatMagic(const std::vector<std::byte>& blob) {
    constexpr std::uint32_t kCatMagic = 19;
    if (blob.size() < sizeof(kCatMagic)) {
        return false;
    }
    std::uint32_t value{};
    std::memcpy(&value, blob.data(), sizeof(value));
    return value == kCatMagic;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const std::filesystem::path configured = argc > 1 ? argv[1] : L"";
    std::string error;
    const auto path = autocattery::snapshot::detail::FindMostRecentSave(
        configured, error);
    if (!path) {
        std::cerr << "save discovery failed: " << error << '\n';
        return 1;
    }
    auto database = autocattery::snapshot::detail::SaveDatabase::OpenReadOnly(
        *path, error);
    if (!database) {
        std::cerr << "SQLite read-only open failed: " << error << '\n';
        return 1;
    }
    std::optional<std::int32_t> day;
    std::vector<autocattery::snapshot::detail::CatStorageRecord> cats;
    std::optional<std::vector<std::byte>> house_state;
    if (!database->ReadCurrentDay(day, error) ||
        !database->ReadCats(cats, error) ||
        !database->ReadHouseState(house_state, error) || !house_state) {
        std::cerr << "verified table read failed: " << error << '\n';
        return 1;
    }
    const auto entries = autocattery::snapshot::ParseHouseState({
        reinterpret_cast<const std::uint8_t*>(house_state->data()),
        house_state->size()});
    if (!entries) {
        std::cerr << "house_state relation parse failed: " << entries.message << '\n';
        return 1;
    }
    std::size_t lz4_encoded{};
    std::size_t raw{};
    for (const auto& cat : cats) {
        if (cat.id <= 0) {
            std::cerr << "cat key identity is invalid\n";
            return 1;
        }
        HasRawCatMagic(cat.blob) ? ++raw : ++lz4_encoded;
    }
    std::cout
        << "container=sqlite"
        << " tables=properties,cats,files"
        << " cat_key_identity=verified"
        << " cat_blobs_raw=" << raw
        << " cat_blobs_lz4=" << lz4_encoded
        << " house_state_room_relation=verified"
        << " house_entries=" << entries.value.size()
        << " day=" << (day ? std::to_string(*day) : "unavailable")
        << '\n';
    return 0;
}
