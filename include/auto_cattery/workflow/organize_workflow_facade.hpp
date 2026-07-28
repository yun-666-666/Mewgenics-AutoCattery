#pragma once

#include "auto_cattery/error.hpp"

namespace autocattery::workflow {

class OrganizeWorkflowFacade {
public:
    virtual ~OrganizeWorkflowFacade() = default;
    virtual Result<void> RequestPreview();
};

}  // namespace autocattery::workflow
