#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"
#include "auto_cattery/execution/domain.hpp"

namespace autocattery::execution {

class IJournalStore {
public:
    virtual ~IJournalStore() = default;
    virtual Result<void> Write(
        const OperationJournalEntry& entry) = 0;
};

class JournalStore final : public IJournalStore {
public:
    explicit JournalStore(std::filesystem::path journal_root);

    Result<void> Write(
        const OperationJournalEntry& entry) override;

private:
    std::filesystem::path journal_root_;
};

}  // namespace autocattery::execution
