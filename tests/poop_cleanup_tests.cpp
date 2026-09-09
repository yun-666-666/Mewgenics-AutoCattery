#include "../src/ui/mew_ui_poop_adapter.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "../third_party/mew_ui_api/src/native/mew_ui_api.h"
#include "test_support.hpp"
#include <array>
#include <cstring>

namespace {
int furniture_type, battle_pickup_type;
int pops;
struct Item { void* unused{}; MewNarrowString key{}; };
struct FurniturePiece {
    alignas(void*) std::array<unsigned char, 0x2E8> data{};
    FurniturePiece(Item* item, void* type = &furniture_type) {
        std::memcpy(data.data(), &type, sizeof(type));
        std::memcpy(data.data() + 0x2D8, &item, sizeof(item));
    }
};
Item MakeItem(const char* key) {
    Item item;
    item.key.size = std::strlen(key);
    std::memcpy(item.key.storage.inline_buf, key, item.key.size + 1);
    item.key.capacity = 15;
    return item;
}
void Pop(void* component) {
    ++pops;
    std::memset(static_cast<unsigned char*>(component) + 0x2D8, 0, sizeof(void*));
}
void NoOp(void*) {}
}

int main() {
    auto poop = MakeItem("poop");
    auto coin = MakeItem("coin");
    auto furniture = MakeItem("poop_statue");
    FurniturePiece first(&poop), second(&poop), other(&poop, &battle_pickup_type);
    FurniturePiece money(&coin), statue(&furniture), removed(&poop), empty(nullptr);
    removed.data[0xF] = 1;
    void* components[] = {nullptr, first.data.data(), money.data.data(),
        other.data.data(), statue.data.data(), removed.data.data(),
        second.data.data(), empty.data.data()};
    auto result = AcMewCleanPoopComponents(components, std::size(components), &furniture_type, Pop);
    AC_CHECK(result.completed && result.cleaned == 2 && pops == 2);
    result = AcMewCleanPoopComponents(components, std::size(components), &furniture_type, Pop);
    AC_CHECK(result.completed && result.cleaned == 0 && pops == 2);
    result = AcMewCleanPoopComponents(nullptr, 0, &furniture_type, Pop);
    AC_CHECK(result.completed && result.cleaned == 0);
    char heap_key[] = "poop";
    poop.key.storage.heap_ptr = heap_key;
    poop.key.capacity = 31;
    FurniturePiece heap(&poop);
    void* heap_components[] = {heap.data.data()};
    result = AcMewCleanPoopComponents(heap_components, 1, &furniture_type, NoOp);
    AC_CHECK(!result.completed && result.cleaned == 0);
    result = AcMewCleanPoopComponents(heap_components, 1, &furniture_type, Pop);
    AC_CHECK(result.completed && result.cleaned == 1);
    AC_CHECK(!AcMewCleanHousePoop(nullptr).completed);
    return autocattery::tests::failures ? 1 : 0;
}
