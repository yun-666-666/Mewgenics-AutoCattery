#include "../src/ui/dead_cat_delivery_service.hpp"
#include "../src/ui/mew_ui_delivery_trace.h"
#include "../src/ui/mew_ui_house_cat_probe.h"
#include "test_support.hpp"
#include <algorithm>

namespace {
std::vector<int64_t> house;
std::vector<int64_t> opened, delivered;
AcDeliveryTrace state;
bool pipe_ready = false;
int open_attempts = 0;

void FinishNativeCleanup() {
    state.completion_delay = 0;
    state.npc_drawer_active = 0;
}
}

extern "C" size_t AcMewCountHouseCats(void*) { return house.size(); }
extern "C" AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void*, const int64_t* ids, size_t count, AcMewHouseCatMatch* matches, size_t capacity) {
    AcMewHouseCatIdentityProbe result{};
    result.house_cat_count = static_cast<uint32_t>(house.size());
    result.stable_bijection = result.consistent_mapping = 1;
    for (size_t i = 0; i < count && i < capacity; ++i) {
        if (std::ranges::find(house, ids[i]) == house.end()) continue;
        matches[result.match_count++] = {ids[i], &state, nullptr};
    }
    return result;
}
extern "C" AcDeliveryTrace AcMewReadDeliveryTrace(void*) { return state; }
extern "C" int AcMewOpenDeadCatPipe(void*, void*, int64_t id) {
    ++open_attempts;
    AC_CHECK(!state.npc_drawer_active && state.completion_delay <= 0);
    if (!pipe_ready) return 2;
    opened.push_back(id);
    state.cat_id = id;
    state.selected_valid = state.npc_drawer_active = 1;
    state.cat_mode = 1;
    return 1;
}
extern "C" int AcMewChooseDeadCatRecipient(void*, int64_t id) {
    AC_CHECK(state.selected_valid && state.cat_id == id);
    delivered.push_back(id);
    std::erase(house, id);
    state.selected_valid = 0;
    state.npc_result = 6;
    state.completion_delay = 0.25;
    return 1;
}
extern "C" int AcMewOpenPopulationCatPipe(void* scene, void* cat, int64_t id) {
    return AcMewOpenDeadCatPipe(scene, cat, id);
}
extern "C" int AcMewChooseTrashRecipient(void* scene, int64_t id) {
    const auto result = AcMewChooseDeadCatRecipient(scene, id);
    // Trash completion has no verified npc_result contract; exact removal
    // and drawer cleanup, rather than Organ Grinder's result 6, prove completion.
    state.npc_result = -1;
    return result;
}
extern "C" int AcMewFindPopulationRecipient(void*, int64_t) { return 7; }
extern "C" int AcMewChoosePopulationRecipient(void* scene, int64_t id, int recipient) {
    AC_CHECK(recipient == 7);
    return AcMewChooseTrashRecipient(scene, id);
}

int main() {
    using namespace autocattery;
    snapshot::HouseSnapshot snapshot;
    house = {9, 2, 4};
    for (const auto id : house) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        cat.life_stage = snapshot::LifeStage::Dead;
        snapshot.cats.push_back(cat);
    }
    state.layout_valid = state.drawer_count = 1;
    ui::DeadCatDeliveryService service;
    AC_CHECK(static_cast<bool>(service.Start(snapshot, house, "unused.sav", {}, {})));
    service.Poll(nullptr);
    service.Poll(nullptr);
    AC_CHECK(service.Active() && delivered.empty() && opened.empty());
    AC_CHECK(open_attempts == 2); // A native no-op stays retryable.
    pipe_ready = true;
    for (const auto id : {2, 4, 9}) {
        service.Poll(nullptr); // Open the current cat only after native cleanup.
        AC_CHECK(opened.back() == id);
        service.Poll(nullptr); // Request recipient; native removes the cat first.
        const auto attempts = open_attempts;
        service.Poll(nullptr);
        service.Poll(nullptr);
        AC_CHECK(service.Active() && open_attempts == attempts);
        state.completion_delay = 0;
        service.Poll(nullptr); // Timer elapsed, but NPC still owns the drawer.
        AC_CHECK(service.Active() && open_attempts == attempts);
        state.npc_drawer_active = 0;
        state.completion_delay = 0.1;
        service.Poll(nullptr); // Inactive alone is not enough either.
        AC_CHECK(service.Active() && open_attempts == attempts);
        FinishNativeCleanup();
        service.Poll(nullptr);
    }
    AC_CHECK(!service.Active() && house.empty());
    AC_CHECK((delivered == std::vector<int64_t>{2, 4, 9}));
    AC_CHECK(service.Message().find("3/3") != std::string::npos);

    // Cancelling while the first native delivery is settling must not open
    // the next cat, close a drawer, or count the pending cat a second time.
    house = {2, 4, 9};
    opened.clear();
    delivered.clear();
    AC_CHECK(static_cast<bool>(service.Start(snapshot, house, "unused.sav", {}, {})));
    service.Poll(nullptr);
    service.Poll(nullptr);
    service.Cancel();
    FinishNativeCleanup();
    service.Poll(nullptr);
    AC_CHECK(!service.Active());
    AC_CHECK((opened == std::vector<int64_t>{2}));
    AC_CHECK((delivered == std::vector<int64_t>{2}));
    house = {2, 4, 9};
    opened.clear();
    delivered.clear();
    for (auto& cat : snapshot.cats) cat.life_stage = snapshot::LifeStage::Adult;
    AC_CHECK(!service.Start(snapshot, {2}, "unused.sav", {}, {}));
    AC_CHECK(static_cast<bool>(service.Start(snapshot, {2, 4}, "unused.sav", {}, {},
        ui::DeadCatDeliveryService::Recipient::Trash)));
    for (const auto id : {2, 4}) {
        service.Poll(nullptr);
        service.Poll(nullptr);
        AC_CHECK(delivered.back() == id);
        service.Poll(nullptr);
        AC_CHECK(service.Active());
        FinishNativeCleanup();
        service.Poll(nullptr);
    }
    AC_CHECK(!service.Active());
    AC_CHECK((house == std::vector<int64_t>{9}));
    return tests::failures ? 1 : 0;
}
