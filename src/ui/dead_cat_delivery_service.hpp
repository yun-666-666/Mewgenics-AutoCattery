#pragma once
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <vector>
#include "auto_cattery/snapshot/domain.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/protection/editor_model.hpp"
namespace autocattery::ui {
std::vector<snapshot::CatId> PlanDeadCatDelivery(const snapshot::HouseSnapshot& snapshot,
    std::span<const protection::ProtectionCatOption> protection);
class DeadCatDeliveryService {
public:
    Result<void> Start(const snapshot::HouseSnapshot& snapshot,
        std::vector<snapshot::CatId> cats, const std::filesystem::path& source,
        const std::filesystem::path& mod_root, const std::filesystem::path& game_root);
    void Poll(void* scene);
    void Cancel();
    bool Active() const { return phase_ != Phase::Idle; }
    const std::string& Message() const { return message_; }
private:
    enum class Phase { Idle, Backup, OpenPipe, Choose, CompleteCat };
    void Stop(std::string message);
    bool Match(void* scene, const std::vector<snapshot::CatId>& ids, void** target = nullptr);
    Phase phase_{Phase::Idle};
    std::future<Result<std::filesystem::path>> backup_;
    std::vector<snapshot::CatId> queue_, expected_;
    std::size_t done_{};
    std::string message_;
    std::chrono::steady_clock::time_point deadline_;
};
}
