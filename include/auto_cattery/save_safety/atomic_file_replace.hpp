#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

class IAtomicFileReplacer {
public:
    virtual ~IAtomicFileReplacer() = default;
    virtual Result<void> ReplaceTemporary(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination) = 0;
};

class AtomicFileReplacer final : public IAtomicFileReplacer {
public:
    Result<void> ReplaceTemporary(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination) override;
};

}  // namespace autocattery::save_safety
