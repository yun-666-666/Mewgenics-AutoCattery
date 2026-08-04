#include "in_game_preview_model.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunInGamePreviewModelTests() {
    workflow::PreviewBundle bundle;
    bundle.snapshot.rooms = {{"Attic", {1, 2}, {}}, {"Floor1_Large", {}, {}}};
    snapshot::CatSnapshot female;
    female.id = 1;
    female.display_name = "Alpha";
    female.sex = snapshot::CatSex::Female;
    female.room_id = "Attic";
    snapshot::CatSnapshot male;
    male.id = 2;
    male.display_name = "Beta";
    male.sex = snapshot::CatSex::Male;
    male.room_id = "Attic";
    bundle.snapshot.cats = {female, male};
    classification::CatDecision decision;
    decision.cat_id = 1;
    decision.combat_score = 42.5;
    bundle.classification.decisions = {decision};
    bundle.room_plan.moves = {{
        1, "Attic", "Floor1_Large", "sex-balance", 0, true}};

    const auto chinese = ui::BuildDetailedPreview(bundle, false);
    AC_CHECK(chinese.pages.size() == 2);
    AC_CHECK(chinese.pages[0].rows.size() == 3);
    AC_CHECK(chinese.pages[0].rows[1].find("2 (母1 公1") !=
             std::string::npos);
    AC_CHECK(chinese.pages[1].rows[0].find("Alpha #1") !=
             std::string::npos);
    AC_CHECK(chinese.pages[1].rows[0].find("平衡繁育房公母比例") !=
             std::string::npos);

    const auto english = ui::BuildDetailedPreview(bundle, true);
    AC_CHECK(english.pages[0].title.find("Room Summary") !=
             std::string::npos);
    AC_CHECK(english.pages[1].rows[0].find("balance the breeding room") !=
             std::string::npos);

    female.room_id.reset();
    bundle.snapshot.cats = {female, male};
    bundle.room_plan.moves = {{
        1, "", "Floor1_Large", "balance-room-population", 0, true}};
    const auto outside = ui::BuildDetailedPreview(bundle, false);
    bool outside_balanced = false;
    for (const auto& row : outside.pages[0].rows) {
        if (row.find("房外") != std::string::npos &&
            row.find("1 (母1") != std::string::npos &&
            row.find("0 (母0") != std::string::npos) {
            outside_balanced = true;
        }
    }
    AC_CHECK(outside_balanced);
}

}  // namespace autocattery::tests
