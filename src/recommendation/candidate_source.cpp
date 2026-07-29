#include "auto_cattery/recommendation/candidate_source.hpp"

namespace autocattery::recommendation {

Result<snapshot::HouseSnapshot>
UnsupportedCombatCandidateSource::CaptureConfirmedCandidates(
    const ui::UiContextSnapshot&) {
    return {
        {},
        ErrorCode::CatDataUnavailable,
        "current ClassChooser candidate identity boundary is unverified"
    };
}

}  // namespace autocattery::recommendation
