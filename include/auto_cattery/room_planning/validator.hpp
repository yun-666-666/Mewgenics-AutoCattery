#pragma once

#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::room_planning {

struct PlanningValidation {
    std::vector<std::string> errors;
    std::vector<std::string> limitations;

    [[nodiscard]] bool Valid() const noexcept {
        return errors.empty();
    }
};

[[nodiscard]] PlanningValidation ValidateInput(
    const RoomPlanningInput& input);

}  // namespace autocattery::room_planning
