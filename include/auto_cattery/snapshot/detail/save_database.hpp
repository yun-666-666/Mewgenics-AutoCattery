#pragma once

#include "auto_cattery/snapshot/domain.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace autocattery::snapshot::detail {

struct sqlite3;

struct CatStorageRecord {
    CatId id{};
    std::vector<std::byte> blob;
};

struct FurnitureStorageRecord {
    std::int64_t key{};
    std::vector<std::byte> blob;
};

class SaveDatabase final {
public:
    ~SaveDatabase();

    SaveDatabase(const SaveDatabase&) = delete;
    SaveDatabase& operator=(const SaveDatabase&) = delete;

    static std::unique_ptr<SaveDatabase> OpenReadOnly(
        const std::filesystem::path& path,
        std::string& error);

    bool ReadCurrentDay(
        std::optional<std::int32_t>& day,
        std::string& error) const;
    bool ReadCats(
        std::vector<CatStorageRecord>& cats,
        std::string& error) const;
    bool ReadHouseState(
        std::optional<std::vector<std::byte>>& blob,
        std::string& error) const;
    bool ReadFurniture(
        std::vector<FurnitureStorageRecord>& furniture,
        std::string& error) const;
    bool ReadFileBlob(
        const char* key,
        std::optional<std::vector<std::byte>>& blob,
        std::string& error) const;

private:
    explicit SaveDatabase(sqlite3* database) noexcept;

    sqlite3* database_{};
};

}  // namespace autocattery::snapshot::detail
