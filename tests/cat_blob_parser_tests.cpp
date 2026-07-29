#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

template<class T>
void Append(std::vector<std::uint8_t>& bytes, const T& value) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

void AppendString(
    std::vector<std::uint8_t>& bytes,
    std::string_view value) {
    Append(bytes, static_cast<std::uint64_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> CatBlob(
    std::string_view class_id = "Colorless",
    std::string_view stat_affinity = "",
    std::int64_t death_day = -1) {
    std::vector<std::uint8_t> bytes(20, 0);
    const std::uint32_t magic = 19;
    const std::uint32_t name_length = 3;
    std::memcpy(bytes.data(), &magic, sizeof(magic));
    std::memcpy(bytes.data() + 12, &name_length, sizeof(name_length));
    for (const wchar_t character : std::wstring_view(L"Mew")) {
        Append(bytes, character);
    }
    AppendString(bytes, stat_affinity);
    bytes.resize(bytes.size() + 16, 0);
    AppendString(bytes, "None");
    bytes.resize(bytes.size() + 368, 0);
    AppendString(bytes, "female31");

    const auto stat_start = bytes.size();
    bytes.resize(bytes.size() + 92, 0);
    for (std::int32_t index = 0; index < 7; ++index) {
        const std::int32_t genetic = index + 1;
        const std::int32_t heredity = index + 11;
        const std::int32_t equipment = index + 21;
        std::memcpy(bytes.data() + stat_start + 8 + index * 4, &genetic, 4);
        std::memcpy(bytes.data() + stat_start + 36 + index * 4, &heredity, 4);
        std::memcpy(bytes.data() + stat_start + 64 + index * 4, &equipment, 4);
    }
    AppendString(bytes, "none");
    bytes.resize(bytes.size() + 14, 0);
    for (int index = 0; index < 10; ++index) {
        AppendString(bytes, index == 0 ? "DefaultMove" : "None");
    }
    for (int index = 0; index < 5; ++index) {
        AppendString(bytes, "None");
        Append(bytes, std::uint32_t{1});
    }
    for (int index = 0; index < 4; ++index) {
        Append(bytes, std::uint8_t{5});
        Append(bytes, std::uint32_t{5});
    }
    AppendString(bytes, class_id);
    const auto post_class_start = bytes.size();
    bytes.resize(bytes.size() + 115, 0);
    const std::int64_t birth_day = 3;
    std::memcpy(
        bytes.data() + post_class_start + 12,
        &birth_day,
        sizeof(birth_day));
    std::memcpy(
        bytes.data() + post_class_start + 20,
        &death_day,
        sizeof(death_day));
    return bytes;
}

}  // namespace

void RunCatBlobParserTests() {
    const auto parsed = snapshot::ParseCatBlob(42, CatBlob(), 17);
    AC_CHECK(static_cast<bool>(parsed));
    AC_CHECK(parsed.value.id == 42);
    AC_CHECK(parsed.value.display_name == "Mew");
    AC_CHECK(parsed.value.raw_ability_slots.size() == 10);
    AC_CHECK(parsed.value.genetic_stats.values[0] == 1);
    AC_CHECK(parsed.value.genetic_stats.values[6] == 7);
    AC_CHECK(parsed.value.heredity_bonus.values[0] == 11);
    AC_CHECK(parsed.value.equipment_bonus.values[6] == 27);
    AC_CHECK(parsed.value.birth_day == 3);
    AC_CHECK(parsed.value.age_days == 14);
    AC_CHECK(parsed.value.life_stage == snapshot::LifeStage::Unknown);
    AC_CHECK(parsed.value.class_id == "Colorless");
    AC_CHECK(
        parsed.value.available_for_combat == snapshot::TriState::Yes);

    const auto classed = snapshot::ParseCatBlob(
        43,
        CatBlob("Hunter"),
        17);
    AC_CHECK(static_cast<bool>(classed));
    AC_CHECK(classed.value.class_id == "Hunter");
    AC_CHECK(
        classed.value.available_for_combat == snapshot::TriState::No);

    const auto variable_metadata = snapshot::ParseCatBlob(
        44,
        CatBlob("Colorless", "dex"),
        17);
    AC_CHECK(static_cast<bool>(variable_metadata));
    AC_CHECK(variable_metadata.value.class_id == "Colorless");

    const auto dead = snapshot::ParseCatBlob(
        45,
        CatBlob("Colorless", "", 12),
        17);
    AC_CHECK(static_cast<bool>(dead));
    AC_CHECK(dead.value.life_stage == snapshot::LifeStage::Dead);
    AC_CHECK(
        dead.value.available_for_combat == snapshot::TriState::No);

    AC_CHECK(!static_cast<bool>(
        snapshot::ParseCatBlob(0, CatBlob(), 17)));

    auto truncated = CatBlob();
    truncated.resize(40);
    AC_CHECK(!static_cast<bool>(
        snapshot::ParseCatBlob(42, truncated, 17)));
}

}  // namespace autocattery::tests
