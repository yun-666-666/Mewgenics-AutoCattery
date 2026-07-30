#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

namespace autocattery::save_safety {

class TestCopyHouseStateStore final {
public:
    TestCopyHouseStateStore();
    explicit TestCopyHouseStateStore(
        const snapshot::detail::WinSqliteApi& write_api);

    [[nodiscard]] Result<std::vector<std::uint8_t>> Read(
        const std::filesystem::path& save) const;
    [[nodiscard]] Result<void> Write(
        const std::filesystem::path& save,
        std::span<const std::uint8_t> house_state) const;

private:
    const snapshot::detail::WinSqliteApi* write_api_{};
};

}  // namespace autocattery::save_safety
