#pragma once

#include <string>
#include <vector>

#include "auto_cattery/workflow/preview_builder.hpp"

namespace autocattery::ui {

struct DetailedPreviewPage {
    std::string title;
    std::string status;
    std::vector<std::string> rows;
};

struct DetailedPreviewModel {
    std::vector<DetailedPreviewPage> pages;
};

[[nodiscard]] DetailedPreviewModel BuildDetailedPreview(
    const workflow::PreviewBundle& bundle,
    bool english);

}  // namespace autocattery::ui
