#include "house_move_probe_session.hpp"

namespace autocattery::ui {

void HouseMoveProbeSession::Arm(
    std::uint64_t scene_generation,
    std::vector<AcMewHouseCatMatch> matches) {
    Cancel();
    if (scene_generation == 0 || matches.empty()) {
        return;
    }
    scene_generation_ = scene_generation;
    matches_ = std::move(matches);
}

void HouseMoveProbeSession::Cancel() noexcept {
    scene_generation_ = 0;
    matches_.clear();
    before_.clear();
}

bool HouseMoveProbeSession::Armed() const noexcept {
    return scene_generation_ != 0 && !matches_.empty();
}

bool HouseMoveProbeSession::HasBeforeSample() const noexcept {
    return before_.size() == matches_.size() && !before_.empty();
}

std::size_t HouseMoveProbeSession::MatchCount() const noexcept {
    return matches_.size();
}

bool HouseMoveProbeSession::CaptureBefore() {
    before_.assign(matches_.size(), {});
    const auto captured = AcMewCaptureHouseMoveSamples(
        matches_.data(),
        matches_.size(),
        before_.data(),
        before_.size());
    if (captured != matches_.size()) {
        before_.clear();
        return false;
    }
    return true;
}

Result<HouseMoveProbeReport> HouseMoveProbeSession::CaptureAfter() {
    if (!HasBeforeSample()) {
        return {{}, ErrorCode::OperationCancelled,
            "the before-drag sample is unavailable"};
    }
    std::vector<AcMewHouseMoveSample> after(matches_.size());
    const auto captured = AcMewCaptureHouseMoveSamples(
        matches_.data(),
        matches_.size(),
        after.data(),
        after.size());
    if (captured != matches_.size()) {
        before_.clear();
        return {{}, ErrorCode::OperationCancelled,
            "the after-drag sample is incomplete"};
    }

    HouseMoveProbeReport report;
    report.scene_generation = scene_generation_;
    report.differences.resize(matches_.size());
    const auto difference_count = AcMewCompareHouseMoveSamples(
        before_.data(),
        before_.size(),
        after.data(),
        after.size(),
        report.differences.data(),
        report.differences.size());
    report.differences.resize(difference_count);
    before_.clear();
    return {std::move(report)};
}

}  // namespace autocattery::ui
