#include "auto_cattery/workflow/organize_workflow_facade.hpp"

namespace autocattery::workflow {

Result<void> OrganizeWorkflowFacade::RequestPreview() {
    return {
        ErrorCode::NotImplemented,
        "The data layer is not connected; no cats were modified."
    };
}

}  // namespace autocattery::workflow
