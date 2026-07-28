#include "auto_cattery/module_registry.hpp"

#include <memory>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

struct Probe {
    int initialized{};
    int shutdown{};
};

class TestModule final : public Module {
public:
    TestModule(const char* name, Probe& probe, bool succeeds = true)
        : name_(name), probe_(probe), succeeds_(succeeds) {}

    const char* Name() const noexcept override {
        return name_;
    }

    bool Initialize(const InitContext&) override {
        ++probe_.initialized;
        return succeeds_;
    }

    void Shutdown() noexcept override {
        ++probe_.shutdown;
    }

private:
    const char* name_;
    Probe& probe_;
    bool succeeds_;
};

}  // namespace

void RunModuleRegistryTests() {
    Probe first;
    Probe second;
    ModuleRegistry registry;
    AC_CHECK(static_cast<bool>(
        registry.Register(std::make_unique<TestModule>("first", first))));
    AC_CHECK(static_cast<bool>(
        registry.Register(std::make_unique<TestModule>("second", second))));
    AC_CHECK(registry.Size() == 2);

    const InitContext context{};
    AC_CHECK(static_cast<bool>(registry.InitializeAll(context)));
    AC_CHECK(first.initialized == 1);
    AC_CHECK(second.initialized == 1);
    registry.ShutdownAll();
    AC_CHECK(first.shutdown == 1);
    AC_CHECK(second.shutdown == 1);

    ModuleRegistry failing;
    Probe succeeds;
    Probe fails;
    AC_CHECK(static_cast<bool>(
        failing.Register(std::make_unique<TestModule>("ok", succeeds))));
    AC_CHECK(static_cast<bool>(
        failing.Register(std::make_unique<TestModule>("fail", fails, false))));
    AC_CHECK(!static_cast<bool>(failing.InitializeAll(context)));
    AC_CHECK(succeeds.shutdown == 1);
    AC_CHECK(fails.shutdown == 0);
}

}  // namespace autocattery::tests
