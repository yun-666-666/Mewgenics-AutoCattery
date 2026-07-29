#include "auto_cattery/execution/game_write_adapter.hpp"

namespace autocattery::execution {
namespace {

Result<void> Unsupported() {
    return {
        ErrorCode::UnsupportedGameBuild,
        "no verified game write mechanism is available"
    };
}

}  // namespace

Result<void> UnsupportedGameWriteAdapter::MoveCat(
    const ApprovedMove&) {
    return Unsupported();
}

Result<void> UnsupportedGameWriteAdapter::CullCat(
    const ApprovedCull&) {
    return Unsupported();
}

Result<void> UnsupportedGameWriteAdapter::Restore(
    const UndoRecord&) {
    return Unsupported();
}

}  // namespace autocattery::execution
