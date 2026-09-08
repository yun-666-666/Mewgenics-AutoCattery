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
    std::int64_t death_day = -1,
    std::string_view voice_id = "female31",
    double sexuality = 0.5) {
    std::vector<std::uint8_t> bytes(20, 0);
    const std::uint32_t magic = 19;
    const std::uint32_t name_length = 3;
    std::memcpy(bytes.data(), &magic, sizeof(magic));
    std::memcpy(bytes.data() + 12, &name_length, sizeof(name_length));
    for (const wchar_t character : std::wstring_view(L"Mew")) {
        Append(bytes, character);
    }
    AppendString(bytes, stat_affinity);
    const auto personality_anchor = bytes.size();
    bytes.resize(bytes.size() + 16, 0);
    AppendString(bytes, "None");
    const auto equipment_start = bytes.size();
    bytes.resize(bytes.size() + 368, 0);
    const std::uint32_t fur_mutation = 300;
    const std::uint32_t body_defect = 700;
    std::memcpy(
        bytes.data() + equipment_start + 68,
        &fur_mutation,
        sizeof(fur_mutation));
    std::memcpy(
        bytes.data() + equipment_start + 68 + 3 * sizeof(std::uint32_t),
        &body_defect,
        sizeof(body_defect));
    AppendString(bytes, voice_id);

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
    std::memcpy(
        bytes.data() + personality_anchor + 40,
        &sexuality,
        sizeof(sexuality));
    return bytes;
}

}  // namespace

void RunCatBlobParserTests() {
    const auto parsed = snapshot::ParseCatBlob(42, CatBlob(), 17);
    AC_CHECK(static_cast<bool>(parsed));
    AC_CHECK(parsed.value.id == 42);
    AC_CHECK(parsed.value.display_name == "Mew");
    AC_CHECK(parsed.value.raw_ability_slots.size() == 10);
    AC_CHECK(parsed.value.raw_visual_part_slots.size() == 15);
    AC_CHECK(parsed.value.raw_visual_part_slots[0].id == 300);
    AC_CHECK(parsed.value.raw_visual_part_slots[1].id == 700);
    AC_CHECK(parsed.value.genetic_stats.values[0] == 1);
    AC_CHECK(parsed.value.genetic_stats.values[6] == 7);
    AC_CHECK(parsed.value.heredity_bonus.values[0] == 11);
    AC_CHECK(parsed.value.equipment_bonus.values[6] == 27);
    AC_CHECK(parsed.value.birth_day == 3);
    AC_CHECK(parsed.value.age_days == 14);
    AC_CHECK(parsed.value.life_stage == snapshot::LifeStage::Adult);
    AC_CHECK(
        parsed.value.available_for_breeding == snapshot::TriState::Yes);
    AC_CHECK(parsed.value.class_id == "Colorless");
    AC_CHECK(parsed.value.sex == snapshot::CatSex::Female);
    AC_CHECK(
        parsed.value.available_for_combat == snapshot::TriState::Yes);

    const auto bisexual = snapshot::ParseCatBlob(
        48, CatBlob("Colorless", "", -1, "female31", 0.5), 17,
        true, true);
    AC_CHECK(static_cast<bool>(bisexual));
    AC_CHECK(
        bisexual.value.sexuality == snapshot::CatSexuality::Bisexual);
    AC_CHECK(bisexual.value.sexuality_coefficient == 0.5);

    const auto hidden = snapshot::ParseCatBlob(
        49, CatBlob(), 17, false, false);
    AC_CHECK(static_cast<bool>(hidden));
    AC_CHECK(
        hidden.value.sexuality == snapshot::CatSexuality::Unknown);
    AC_CHECK(!hidden.value.genetic_stats.values[0].has_value());

    const auto classed = snapshot::ParseCatBlob(
        43,
        CatBlob("Hunter"),
        17);
    AC_CHECK(static_cast<bool>(classed));
    AC_CHECK(classed.value.class_id == "Hunter");
    for (const std::uint64_t payload_size : {1ULL, 4ULL}) {
        auto history = CatBlob("Hunter");
        const auto tail = history.size() - 115;
        std::memcpy(history.data() + tail + 28, &payload_size, sizeof(payload_size));
        history.insert(history.begin() + tail + 36, static_cast<std::size_t>(payload_size), 4);
        const std::int32_t old_state = 2;
        std::memcpy(history.data() + tail + 36 + payload_size, &old_state, sizeof(old_state));
        const auto with_history = snapshot::ParseCatBlob(45, history, 17);
        AC_CHECK(static_cast<bool>(with_history));
        AC_CHECK(with_history.value.class_id == "Hunter");
        AC_CHECK(with_history.value.birth_day == 3);
        AC_CHECK(with_history.value.life_stage == snapshot::LifeStage::Senior);
        history.pop_back();
        AC_CHECK(!snapshot::ParseCatBlob(45, history, 17));
    }
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
    auto senior_blob = CatBlob();
    const std::int32_t old_state = 2;
    std::memcpy(senior_blob.data() + senior_blob.size() - 115 + 36,
                &old_state, sizeof(old_state));
    const auto senior = snapshot::ParseCatBlob(48, senior_blob, 17);
    AC_CHECK(static_cast<bool>(senior));
    AC_CHECK(senior.value.life_stage == snapshot::LifeStage::Senior);
    AC_CHECK(senior.value.available_for_breeding == snapshot::TriState::No);
    AC_CHECK(parsed.value.life_stage == snapshot::LifeStage::Adult);
    auto dead_senior_blob = CatBlob("Colorless", "", 12);
    std::memcpy(dead_senior_blob.data() + dead_senior_blob.size() - 115 + 36,
                &old_state, sizeof(old_state));
    const auto dead_senior = snapshot::ParseCatBlob(49, dead_senior_blob, 17);
    AC_CHECK(static_cast<bool>(dead_senior));
    AC_CHECK(dead_senior.value.life_stage == snapshot::LifeStage::Dead);
    AC_CHECK(
        dead.value.available_for_combat == snapshot::TriState::No);

    const auto male = snapshot::ParseCatBlob(
        46,
        CatBlob("Colorless", "", -1, "male12"),
        17);
    AC_CHECK(static_cast<bool>(male));
    AC_CHECK(male.value.sex == snapshot::CatSex::Male);

    const auto unknown_sex = snapshot::ParseCatBlob(
        47,
        CatBlob("Colorless", "", -1, "voice_custom"),
        17);
    AC_CHECK(static_cast<bool>(unknown_sex));
    AC_CHECK(unknown_sex.value.sex == snapshot::CatSex::Unknown);

    AC_CHECK(!static_cast<bool>(
        snapshot::ParseCatBlob(0, CatBlob(), 17)));

    auto truncated = CatBlob();
    truncated.resize(40);
    AC_CHECK(!static_cast<bool>(
        snapshot::ParseCatBlob(42, truncated, 17)));
}

}  // namespace autocattery::tests
