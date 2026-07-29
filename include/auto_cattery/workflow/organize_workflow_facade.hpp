#pragma once

#include <cstdint>
#include <memory>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/game_read_adapter.hpp"

namespace autocattery::workflow {

class OrganizeWorkflowFacade {
public:
    explicit OrganizeWorkflowFacade(
        std::unique_ptr<snapshot::IGameReadAdapter> read_adapter = {});
    virtual ~OrganizeWorkflowFacade() = default;
    virtual Result<void> RequestPreview(std::uint64_t scene_generation);

private:
    std::unique_ptr<snapshot::IGameReadAdapter> read_adapter_;
};

}  // namespace autocattery::workflow
