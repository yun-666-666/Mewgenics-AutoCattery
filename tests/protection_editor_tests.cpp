#include "auto_cattery/protection/editor_model.hpp"
#include "auto_cattery/protection/identity.hpp"
#include "auto_cattery/protection/sidecar.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot House(
    std::string save_name,
    std::int64_t birth_day,
    std::string cat_name) {
    snapshot::HouseSnapshot house;
    house.source_save_name = std::move(save_name);
    house.capabilities.stable_cat_id = true;
    house.rooms = {{.id = "Attic"}, {.id = "Floor1_Large"}};
    snapshot::CatSnapshot cat;
    cat.id = 7;
    cat.display_name = std::move(cat_name);
    cat.breed_id = "standard";
    cat.voice_id = "female_1";
    cat.birth_day = birth_day;
    cat.room_id = "Floor1_Large";
    house.cats.push_back(std::move(cat));
    return house;
}

std::filesystem::path TestPath() {
    return std::filesystem::temp_directory_path() /
        ("auto_cattery_protection_editor_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()) +
         ".json");
}

}  // namespace

void RunProtectionEditorTests() {
    auto first = House("campaign-a.sav", 3, "Alpha");
    auto moved = first.cats.front();
    moved.display_name = "Renamed Alpha";
    moved.room_id = "Attic";
    AC_CHECK(
        protection::StableCatIdentityToken(first.cats.front()) ==
        protection::StableCatIdentityToken(moved));
    moved.birth_day = 4;
    AC_CHECK(
        protection::StableCatIdentityToken(first.cats.front()) !=
        protection::StableCatIdentityToken(moved));

    const auto path = TestPath();
    {
        std::ofstream output(path);
        output << R"({"schema_version":1,"records":[],"blacklist":[]})";
    }
    std::vector snapshots{
        first,
        House("campaign-b.sav", 9, "Beta")
    };
    protection::ProtectionEditorModel model(path, snapshots);
    AC_CHECK(static_cast<bool>(model.Reload()));
    AC_CHECK(model.saves().size() == 2);
    AC_CHECK(model.cats().size() == 1);
    AC_CHECK(static_cast<bool>(model.Apply(
        0, protection::ProtectionLevel::NoMove,
        snapshot::RoomId{"Attic"})));
    AC_CHECK(model.cats().front().level ==
             protection::ProtectionLevel::NoMove);
    AC_CHECK(model.cats().front().fixed_room == "Attic");

    AC_CHECK(static_cast<bool>(model.SelectSave(1)));
    AC_CHECK(model.cats().size() == 1);
    AC_CHECK(model.cats().front().display_name == "Beta");
    AC_CHECK(!model.cats().front().level);
    AC_CHECK(static_cast<bool>(model.Apply(
        0, protection::ProtectionLevel::NoCull, std::nullopt)));
    const auto two_records = protection::LoadProtectionSidecar(path);
    AC_CHECK(two_records.status ==
             protection::SidecarLoadStatus::Loaded);
    AC_CHECK(two_records.records.size() == 2);
    AC_CHECK(two_records.records[0].protection.cat_id == 7);
    AC_CHECK(two_records.records[1].protection.cat_id == 7);

    AC_CHECK(static_cast<bool>(model.Remove(0)));
    const auto one_record = protection::LoadProtectionSidecar(path);
    AC_CHECK(one_record.records.size() == 1);
    AC_CHECK(one_record.records.front().display_name == "Alpha");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

}  // namespace autocattery::tests
