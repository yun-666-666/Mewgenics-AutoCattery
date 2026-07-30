#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

class StableSaveGuard final {
public:
    StableSaveGuard() = default;
    ~StableSaveGuard();
    StableSaveGuard(const StableSaveGuard&) = delete;
    StableSaveGuard& operator=(const StableSaveGuard&) = delete;
    StableSaveGuard(StableSaveGuard&& other) noexcept;
    StableSaveGuard& operator=(StableSaveGuard&& other) noexcept;

    [[nodiscard]] static Result<StableSaveGuard> Acquire(
        const std::filesystem::path& save,
        std::chrono::milliseconds stable_window =
            std::chrono::milliseconds(150));

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }
    [[nodiscard]] std::uintmax_t ByteSize() const noexcept {
        return byte_size_;
    }

private:
    void Close() noexcept;

    void* handle_{};
    std::filesystem::path path_;
    std::uintmax_t byte_size_{};
};

}  // namespace autocattery::save_safety
