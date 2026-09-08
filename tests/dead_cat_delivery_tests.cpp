#include "../src/ui/dead_cat_delivery_service.hpp"
#include "test_support.hpp"
namespace autocattery::tests {
void RunDeadCatDeliveryTests() {
    snapshot::HouseSnapshot snapshot;
    for (const auto id : {9, 2, 7, 4}) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        cat.life_stage = id == 7 ? snapshot::LifeStage::Senior : snapshot::LifeStage::Dead;
        snapshot.cats.push_back(cat);
    }
    std::vector<protection::ProtectionCatOption> options;
    for (const auto id : {9, 2, 7, 4}) {
        protection::ProtectionCatOption option;
        option.cat_id = id;
        options.push_back(option);
    }
    auto ids = ui::PlanDeadCatDelivery(snapshot, options);
    AC_CHECK((ids == std::vector<snapshot::CatId>{2, 4, 9}));
    options[0].level = protection::ProtectionLevel::NoCull;
    options[3].fixed_room = "Attic";
    ids = ui::PlanDeadCatDelivery(snapshot, options);
    AC_CHECK((ids == std::vector<snapshot::CatId>{2}));
    options.erase(options.begin() + 1);
    AC_CHECK(ui::PlanDeadCatDelivery(snapshot, options).empty());
    ui::DeadCatDeliveryService service;
    AC_CHECK(!service.Start(snapshot, {7}, "unused.sav", {}, {}));
    AC_CHECK(!service.Active());
}
}
