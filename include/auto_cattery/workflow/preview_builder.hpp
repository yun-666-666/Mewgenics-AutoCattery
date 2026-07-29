#pragma once

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/config.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/protection/domain.hpp"
#include "auto_cattery/snapshot/game_read_adapter.hpp"
#include "auto_cattery/workflow/domain.hpp"
#include "auto_cattery/workflow/state_machine.hpp"

namespace autocattery::workflow {

struct PreviewBundle {
  OrganizePreview preview;
  snapshot::HouseSnapshot snapshot;
  classification::ClassificationPlan classification;
  std::vector<protection::ProtectionDecision> protections;
  room_planning::RoomPlan room_plan;
};

class PreviewBuilder final {
public:
  PreviewBuilder(snapshot::IGameReadAdapter &read_adapter, Config config = {});

  [[nodiscard]] Result<PreviewBundle> Build(std::uint64_t scene_generation,
                                            WorkflowCapability capability,
                                            WorkflowStateMachine &state) const;

private:
  snapshot::IGameReadAdapter &read_adapter_;
  Config config_;
};

} // namespace autocattery::workflow
