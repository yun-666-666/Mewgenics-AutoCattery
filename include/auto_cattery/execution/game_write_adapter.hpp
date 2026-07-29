#pragma once

#include "auto_cattery/error.hpp"
#include "auto_cattery/execution/domain.hpp"

namespace autocattery::execution {

enum class WriteCapability {
    Unsupported,
    MoveOnly,
    MoveAndCull
};

class IGameWriteAdapter {
public:
    virtual ~IGameWriteAdapter() = default;
    [[nodiscard]] virtual WriteCapability Capability() const noexcept = 0;
    virtual Result<void> MoveCat(const ApprovedMove& move) = 0;
    virtual Result<void> CullCat(const ApprovedCull& cull) = 0;
    virtual Result<void> Restore(const UndoRecord& record) = 0;
};

class UnsupportedGameWriteAdapter final : public IGameWriteAdapter {
public:
    [[nodiscard]] WriteCapability Capability() const noexcept override {
        return WriteCapability::Unsupported;
    }
    Result<void> MoveCat(const ApprovedMove&) override;
    Result<void> CullCat(const ApprovedCull&) override;
    Result<void> Restore(const UndoRecord&) override;
};

}  // namespace autocattery::execution
