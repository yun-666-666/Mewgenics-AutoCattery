#include "auto_cattery/diagnostics/cat_data_collector.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "test_support.hpp"

namespace autocattery::tests {

void RunCatDataCollectorTests() {
    const auto root = std::filesystem::temp_directory_path() /
        "auto_cattery_cat_data_collector_tests";
    std::filesystem::create_directories(root);

    workflow::PreviewBundle bundle;
    bundle.snapshot.snapshot_id = 7;
    bundle.snapshot.source_save_name = "private-save-name.sav";
    bundle.snapshot.captured_at = std::chrono::system_clock::now();
    snapshot::CatSnapshot cat;
    cat.id = 17;
    cat.display_name = "private-player-cat-name";
    cat.sex = snapshot::CatSex::Female;
    cat.room_id = "Attic";
    cat.genetic_stats.values[0] = 7;
    bundle.snapshot.cats.push_back(cat);
    bundle.snapshot.rooms.push_back({"Attic", {17}, {}});
    classification::CatDecision decision;
    decision.cat_id = 17;
    decision.combat_score = 9.5;
    bundle.classification.decisions.push_back(decision);
    bundle.preview.bindings.snapshot_content_digest = "test-digest";

    const auto written = diagnostics::WriteCatDataSnapshot(bundle, root);
    AC_CHECK(static_cast<bool>(written));
    std::ifstream stream(written.value, std::ios::binary);
    nlohmann::json json;
    stream >> json;
    AC_CHECK(json.at("privacy").at("opt_in_required") == true);
    AC_CHECK(json.at("cats").at(0).at("cat_id") == 17);
    AC_CHECK(json.at("cats").at(0).at("genetic_stats").at(0) == 7);
    const auto serialized = json.dump();
    AC_CHECK(serialized.find("private-player-cat-name") == std::string::npos);
    AC_CHECK(serialized.find("private-save-name.sav") == std::string::npos);
}

}  // namespace autocattery::tests
