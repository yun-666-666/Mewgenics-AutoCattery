#include "dead_cat_delivery_service.hpp"
#include "mew_ui_delivery_trace.h"
#include "mew_ui_house_cat_probe.h"
#include "mew_ui_population_recipient_policy.h"
#include "test_support.hpp"
#include <algorithm>
#include <array>

namespace {
std::vector<int64_t> cats;
std::vector<int> deliveries;
AcDeliveryTrace state;
std::array<int, 7> quotas{};
bool available = true;
int opens = 0;
}
extern "C" size_t AcMewCountHouseCats(void*) { return cats.size(); }
extern "C" AcMewHouseCatIdentityProbe AcMewProbeHouseCatIdentity(
    void*, const int64_t* ids, size_t count, AcMewHouseCatMatch* matches, size_t capacity) {
    AcMewHouseCatIdentityProbe result{};
    result.house_cat_count = static_cast<uint32_t>(cats.size());
    result.stable_bijection = result.consistent_mapping = 1;
    for (size_t i = 0; i < count && i < capacity; ++i)
        if (std::ranges::find(cats, ids[i]) != cats.end())
            matches[result.match_count++] = {ids[i], &state, nullptr};
    return result;
}
extern "C" AcDeliveryTrace AcMewReadDeliveryTrace(void*) { return state; }
extern "C" int AcMewOpenPopulationCatPipe(void*, void*, int64_t id) {
    AC_CHECK(!state.selected_valid && !state.npc_drawer_active && state.completion_delay <= 0);
    ++opens;
    state.cat_id = id;
    state.cat_mode = state.selected_valid = state.npc_drawer_active = 1;
    return 1;
}
extern "C" int AcMewOpenDeadCatPipe(void*, void*, int64_t) { return 0; }
extern "C" int AcMewChooseDeadCatRecipient(void*, int64_t) { return 0; }
extern "C" int AcMewChooseTrashRecipient(void*, int64_t) { return 0; }
extern "C" int AcMewFindPopulationRecipient(void*, int64_t id) {
    if (!available || !state.selected_valid || state.cat_id != id) return -1;
    uint32_t accepts = 1U << 7;
    for (int npc = 0; npc < 7; ++npc)
        if (quotas[npc] > 0 && (id != 2 || npc == 4)) accepts |= 1U << npc;
    return AcMewSelectPopulationRecipient(accepts, 0);
}
extern "C" int AcMewChoosePopulationRecipient(void* scene, int64_t id, int npc) {
    AC_CHECK(AcMewFindPopulationRecipient(scene, id) == npc);
    if (npc < 7) --quotas[npc];
    deliveries.push_back(npc);
    std::erase(cats, id);
    state.selected_valid = 0;
    state.npc_result = npc == 7 ? -1 : npc;
    state.completion_delay = 0.25;
    return 1;
}
int main() {
    using namespace autocattery;
    const auto mask = [](std::initializer_list<int> recipients) {
        uint32_t value = 1U << 7;
        for (const auto npc : recipients) value |= 1U << npc;
        return value;
    };
    AC_CHECK(AcMewSelectPopulationRecipient(mask({3}), 0) == 3); // Retired -> Frank.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({1, 3}), 0) == 1); // Butch exception.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({0, 2}), 0) == 0); // Doctor before Tink.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({0, 1}), 0) == 1); // Butch before doctor.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({2, 4, 5}), 0) == 2);
    AC_CHECK(AcMewSelectPopulationRecipient(mask({4, 5}), 0) == 5); // Either rank-4 is fine.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({0, 1, 2, 3, 4, 5, 6}), 1) == 6);
    AC_CHECK(AcMewSelectPopulationRecipient(mask({0, 1, 3}), 1) == -1); // Dead never trash.
    AC_CHECK(AcMewSelectPopulationRecipient(mask({6}), 0) == 7); // Living never Organ Grinder.
    AC_CHECK(AcMewSelectPopulationRecipient(0, 0) == -1);
    // All combinations of currently eligible living recipients obey the same
    // priority; no specific cat identity or quota is built into that policy.
    const std::array order{1, 3, 0, 2, 5, 4};
    for (uint32_t accepts = 0; accepts < 64; ++accepts) {
        int expected = 7;
        for (const auto npc : order)
            if (accepts & (1U << npc)) { expected = npc; break; }
        AC_CHECK(AcMewSelectPopulationRecipient(accepts | (1U << 7), 0) == expected);
    }
    ui::DeadCatDeliveryService service;
    snapshot::HouseSnapshot snapshot;
    for (int id = 1; id <= 4; ++id) snapshot.cats.push_back({.id = id,
        .life_stage = snapshot::LifeStage::Adult});
    const auto start = [&] {
        cats = {1, 2, 3, 4};
        state = {};
        state.layout_valid = state.drawer_count = 1;
        deliveries.clear();
        opens = 0;
        available = true;
        AC_CHECK(static_cast<bool>(service.Start(snapshot, cats, "fixture.sav", {}, {},
            ui::DeadCatDeliveryService::Recipient::NpcPreferred)));
    };
    start();
    quotas = {1, 0, 0, 1, 1, 0, 0};
    for (int id = 1; id <= 4; ++id) {
        service.Poll(nullptr); // Native cat pipe.
        service.Poll(nullptr); // Recipient preview without donation.
        AC_CHECK(deliveries.size() == static_cast<size_t>(id - 1));
        service.Poll(nullptr); // Donation uses current acceptance/quotas.
        const auto opened = opens;
        service.Poll(nullptr);
        AC_CHECK(opens == opened && service.Active());
        state.npc_drawer_active = 0;
        state.completion_delay = 0;
        service.Poll(nullptr);
    }
    AC_CHECK(!service.Active() && cats.empty());
    AC_CHECK((deliveries == std::vector<int>{3, 4, 0, 7}));
    start();
    quotas = {1};
    service.Poll(nullptr);
    service.Poll(nullptr);
    service.Cancel(); // F10 before recipient confirmation.
    service.Poll(nullptr);
    AC_CHECK(deliveries.empty() && cats.size() == 4 && !service.Active());
    start();
    service.Poll(nullptr);
    available = false;
    service.Poll(nullptr);
    AC_CHECK(!service.Active() && deliveries.empty()); // Never fallback on error.
    start();
    quotas = {1};
    service.Poll(nullptr);
    service.Poll(nullptr);
    quotas = {0, 1}; // Acceptance changed between preview and confirmation.
    service.Poll(nullptr);
    AC_CHECK(deliveries.empty());
    service.Poll(nullptr);
    service.Poll(nullptr);
    AC_CHECK((deliveries == std::vector<int>{1}));
    state.npc_result = 3; // A mismatched native result stops the queue.
    service.Poll(nullptr);
    AC_CHECK(!service.Active() && opens == 1);
    return tests::failures ? 1 : 0;
}
