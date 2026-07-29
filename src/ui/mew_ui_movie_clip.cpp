#include "mew_ui_movie_clip.hpp"

#include <cstddef>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::size_t kMovieClipStateFlagsOffset = 0x09U;
constexpr unsigned char kMovieClipPlayingBit = 0x02U;

}  // namespace

bool HoldMewUiMovieClipFrame(void* movie_clip, int frame) noexcept {
    if (MewUI_PlayMovieClipFrame(movie_clip, frame) == 0) {
        return false;
    }
    __try {
        // Retains the goto-and-stop behavior verified by the Stage 12 view.
        auto* flags = static_cast<unsigned char*>(movie_clip) +
            kMovieClipStateFlagsOffset;
        *flags &= static_cast<unsigned char>(~kMovieClipPlayingBit);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace autocattery::ui
