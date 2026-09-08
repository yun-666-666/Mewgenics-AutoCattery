#include "dead_cat_delivery_service.hpp"
#include <algorithm>
#include "auto_cattery/logger.hpp"
#include "auto_cattery/save_safety/save_stability.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "mew_ui_house_cat_probe.h"
#include "mew_ui_delivery_trace.h"
namespace autocattery::ui {
std::vector<snapshot::CatId> PlanDeadCatDelivery(const snapshot::HouseSnapshot& snapshot,
    std::span<const protection::ProtectionCatOption> options) {
    std::vector<snapshot::CatId> ids;
    for (const auto& cat : snapshot.cats) {
        if (cat.life_stage != snapshot::LifeStage::Dead) continue;
        const auto option = std::ranges::find(options, cat.id, &protection::ProtectionCatOption::cat_id);
        if (option == options.end() || option->fixed_room ||
            (option->level && *option->level != protection::ProtectionLevel::None)) continue;
        ids.push_back(cat.id);
    }
    std::ranges::sort(ids);
    return ids;
}

Result<void> DeadCatDeliveryService::Start(const snapshot::HouseSnapshot& snapshot,
    std::vector<snapshot::CatId> cats, const std::filesystem::path& source,
    const std::filesystem::path& mod_root, const std::filesystem::path& game_root) {
    if (Active() || cats.empty() || source.empty()) {
        message_ = "没有可交付猫或存档尚未确认";
        return {ErrorCode::CatDataUnavailable, message_};
    }
    for (const auto id : cats) {
        const auto found = std::ranges::find(snapshot.cats, id, &snapshot::CatSnapshot::id);
        if (found == snapshot.cats.end() || found->life_stage != snapshot::LifeStage::Dead)
            return {ErrorCode::CatDataUnavailable, "交付队列包含非死亡猫"};
    }
    queue_ = std::move(cats);
    std::ranges::sort(queue_);
    expected_.clear();
    for (const auto& cat : snapshot.cats) expected_.push_back(cat.id);
    done_ = 0;
    phase_ = Phase::Backup;
    message_ = "正在保存恢复副本；尚未交付";
    const auto ids = expected_;
    backup_ = std::async(std::launch::async, [source, mod_root, game_root, ids]()
        -> Result<std::filesystem::path> {
        try {
            const auto guard = save_safety::StableSaveGuard::Acquire(source,
                std::chrono::milliseconds(150));
            if (!guard) return {{}, guard.code, guard.message};
            const auto folder = mod_root / L"backups" / (L"dead-delivery-" +
                std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(folder);
            const auto destination = folder / source.filename();
            std::filesystem::copy_file(source, destination);
            snapshot::SaveSnapshotAdapter adapter(destination, game_root);
            const auto saved = adapter.CaptureHouseSnapshot(1);
            if (!saved) return {{}, saved.code, saved.message};
            std::vector<snapshot::CatId> actual;
            for (const auto& cat : saved.value.cats) actual.push_back(cat.id);
            auto expected = ids;
            std::ranges::sort(actual); std::ranges::sort(expected);
            if (actual != expected) return {{}, ErrorCode::CatDataUnavailable,
                "存档猫群已改变，请正常保存后重新预览"};
            return {destination};
        } catch (const std::exception& e) {
            return {{}, ErrorCode::BackupFailed, e.what()};
        }
    });
    return {};
}
void DeadCatDeliveryService::Stop(std::string message) {
    phase_ = Phase::Idle;
    message_ = std::move(message) + "（已交付 " + std::to_string(done_) +
        "/" + std::to_string(queue_.size()) + "）";
    Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19202", message_);
}
void DeadCatDeliveryService::Cancel() {
    if (Active()) Stop("已停止后续交付；已开始的单只交付可能完成");
}
bool DeadCatDeliveryService::Match(void* scene,
    const std::vector<snapshot::CatId>& ids, void** target) {
    if (ids.empty()) return AcMewCountHouseCats(scene) == 0;
    std::vector<AcMewHouseCatMatch> matches(ids.size());
    const auto result = AcMewProbeHouseCatIdentity(scene, ids.data(), ids.size(),
        matches.data(), matches.size());
    if (!result.stable_bijection || !result.consistent_mapping ||
        result.match_count != ids.size() || result.house_cat_count != ids.size()) return false;
    if (target) {
        const auto found = std::ranges::find(matches, queue_[done_], &AcMewHouseCatMatch::cat_id);
        if (found == matches.end()) return false;
        *target = found->component;
    }
    return true;
}
void DeadCatDeliveryService::Poll(void* scene) {
    if (!Active()) return;
    const auto now = std::chrono::steady_clock::now();
    if (phase_ == Phase::Backup) {
        if (backup_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        const auto backup = backup_.get();
        if (!backup) { Stop("恢复副本失败：" + backup.message); return; }
        Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19200",
            "Recovery copy ready under backups/" + backup.value.parent_path().filename().string());
        phase_ = Phase::OpenPipe;
    }
    if (phase_ == Phase::OpenPipe) {
        void* cat{};
        if (!Match(scene, expected_, &cat)) { Stop("猫群变化，请重新预览"); return; }
        if (AcMewOpenDeadCatPipe(scene, cat, queue_[done_]) != 1) {
            Stop("原生管道入口未就绪，未继续交付"); return;
        }
        deadline_ = now + std::chrono::seconds(15);
        phase_ = Phase::Choose;
        return;
    }
    const auto state = AcMewReadDeliveryTrace(scene);
    if (!state.layout_valid || state.seh_code || state.drawer_count != 1) {
        Stop("原生交付状态不可用"); return;
    }
    if (now > deadline_) { Stop("等待原生交付状态超时，停止后续交付"); return; }
    if (phase_ == Phase::Choose) {
        if (!state.cat_mode || !state.selected_valid) return;
        if (state.cat_id != queue_[done_] ||
            AcMewChooseDeadCatRecipient(scene, queue_[done_]) != 1) {
            Stop("待交付猫或目标界面变化"); return;
        }
        Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19201",
            "Requested Organ Grinder delivery cat_id=" + std::to_string(queue_[done_]));
        phase_ = Phase::CompleteCat;
        return;
    }
    if (state.selected_valid || state.callback_rva) return;
    if (state.npc_result != 6) { Stop("NPC交付结果与预期不符"); return; }
    auto remaining = expected_;
    std::erase(remaining, queue_[done_]);
    if (!Match(scene, remaining)) return;
    expected_ = std::move(remaining);
    ++done_;
    Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19203",
        "Completed Organ Grinder delivery cat_id=" + std::to_string(queue_[done_ - 1]));
    if (done_ == queue_.size()) Stop("死亡猫交付完成");
    else phase_ = Phase::OpenPipe;
}
}
