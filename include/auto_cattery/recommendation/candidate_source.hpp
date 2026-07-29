#pragma once

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::recommendation {

class ICurrentCombatCandidateSource {
public:
    virtual ~ICurrentCombatCandidateSource() = default;
    virtual Result<snapshot::HouseSnapshot> CaptureConfirmedCandidates(
        const ui::UiContextSnapshot& context) = 0;
};

class UnsupportedCombatCandidateSource final
    : public ICurrentCombatCandidateSource {
public:
    Result<snapshot::HouseSnapshot> CaptureConfirmedCandidates(
        const ui::UiContextSnapshot& context) override;
};

}  // namespace autocattery::recommendation
