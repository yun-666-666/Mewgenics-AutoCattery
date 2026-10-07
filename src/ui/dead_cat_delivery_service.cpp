#include "dead_cat_delivery_service.hpp"
#include <algorithm>
#include "auto_cattery/logger.hpp"
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
    const std::filesystem::path&, const std::filesystem::path&, Recipient recipient) {
    if (Active() || cats.empty() || source.empty()) {
        message_ = "没有可交付猫或存档尚未确认";
        return {ErrorCode::CatDataUnavailable, message_};
    }
    for (const auto id : cats) {
        const auto found = std::ranges::find(snapshot.cats, id, &snapshot::CatSnapshot::id);
        if (found == snapshot.cats.end() ||
            (recipient == Recipient::OrganGrinder && found->life_stage != snapshot::LifeStage::Dead))
            return {ErrorCode::CatDataUnavailable, "交付队列包含非死亡猫"};
    }
    queue_ = std::move(cats);
    recipient_ = recipient;
    std::ranges::sort(queue_);
    expected_.clear();
    for (const auto& cat : snapshot.cats) expected_.push_back(cat.id);
    done_ = 0;
    phase_ = Phase::OpenPipe;
    deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    message_ = "正在按编号顺序交付";
    Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19200",
        "Started sequential delivery; automatic backup disabled; cats=" + std::to_string(queue_.size()));
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
    if (now > deadline_) { Stop("等待原生交付状态超时，停止后续交付"); return; }
    if (phase_ == Phase::OpenPipe) {
        void* cat{};
        if (!Match(scene, expected_, &cat)) { Stop("猫群变化，请重新预览"); return; }
        const auto opened = recipient_ != Recipient::OrganGrinder
            ? AcMewOpenPopulationCatPipe(scene, cat, queue_[done_])
            : AcMewOpenDeadCatPipe(scene, cat, queue_[done_]);
        if (opened == 2) return;
        if (opened != 1) {
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
    if (phase_ == Phase::Choose) {
        if (!state.cat_mode || !state.selected_valid) return;
        if (state.cat_id != queue_[done_]) { Stop("待交付猫变化"); return; }
        if (recipient_ == Recipient::NpcPreferred) {
            chosen_recipient_ = AcMewFindPopulationRecipient(scene, queue_[done_]);
            if (chosen_recipient_ < 0) { Stop("NPC接收条件不可用，未继续交付"); return; }
            message_ = chosen_recipient_ == 7 ? "当前没有NPC接收，下一步送入垃圾桶" :
                "当前接收NPC编号 " + std::to_string(chosen_recipient_) + "；可按F10停止";
            Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19205",
                "Recipient preview cat_id=" + std::to_string(queue_[done_]) +
                " recipient=" + std::to_string(chosen_recipient_));
            phase_ = Phase::ConfirmRecipient;
            return;
        }
        const auto chosen = recipient_ == Recipient::Trash
            ? AcMewChooseTrashRecipient(scene, queue_[done_])
            : AcMewChooseDeadCatRecipient(scene, queue_[done_]);
        if (chosen != 1) {
            Stop("待交付猫或目标界面变化"); return;
        }
        Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19201",
            std::string(recipient_ == Recipient::Trash ? "Requested trash cat_id=" :
                "Requested Organ Grinder delivery cat_id=") + std::to_string(queue_[done_]));
        phase_ = Phase::CompleteCat;
        return;
    }
    if (phase_ == Phase::ConfirmRecipient) {
        const auto current = AcMewFindPopulationRecipient(scene, queue_[done_]);
        if (current < 0) { Stop("NPC接收条件不可用，未继续交付"); return; }
        if (current != chosen_recipient_) { phase_ = Phase::Choose; return; }
        if (AcMewChoosePopulationRecipient(scene, queue_[done_], current) != 1) {
            Stop("待交付猫或NPC接收条件变化"); return;
        }
        Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19201",
            "Requested population delivery cat_id=" + std::to_string(queue_[done_]) +
            " recipient=" + std::to_string(current));
        phase_ = Phase::CompleteCat;
        return;
    }
    if (state.selected_valid || state.callback_rva) return;
    if ((recipient_ == Recipient::OrganGrinder && state.npc_result != 6) ||
        (recipient_ == Recipient::NpcPreferred && chosen_recipient_ < 7 &&
            state.npc_result != chosen_recipient_)) {
        Stop("NPC交付结果与预期不符"); return;
    }
    auto remaining = expected_;
    std::erase(remaining, queue_[done_]);
    if (!Match(scene, remaining)) return;
    // Cat removal precedes native drawer cleanup. Opening the next details
    // during that interval loses drawer ownership and races the return path.
    if (state.npc_drawer_active || !(state.completion_delay <= 0.0)) return;
    expected_ = std::move(remaining);
    ++done_;
    Logger::Instance().Write(LogLevel::Info, "DeadCatDelivery", "AC19203",
        "Completed delivery cat_id=" + std::to_string(queue_[done_ - 1]) +
        " recipient=" + std::to_string(recipient_ == Recipient::NpcPreferred ?
            chosen_recipient_ : recipient_ == Recipient::Trash ? 7 : 6));
    if (done_ == queue_.size()) Stop(recipient_ != Recipient::OrganGrinder ? "超额猫交付完成" : "死亡猫交付完成");
    else {
        phase_ = Phase::OpenPipe;
        deadline_ = now + std::chrono::seconds(15);
    }
}
}
