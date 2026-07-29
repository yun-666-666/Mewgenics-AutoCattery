#include "auto_cattery/workflow/preview_store.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWorkflowPreviewStoreTests() {
  using namespace std::chrono_literals;
  auto now = std::chrono::steady_clock::time_point{} + 1s;
  workflow::PreviewStore store([&now] { return now; });

  workflow::PreviewBundle first;
  first.preview.expiration.expires_at = now + 2min;
  first.preview.bindings.scene_generation = 4;
  first.preview.bindings.snapshot_content_digest = "snapshot";
  first.preview.bindings.classification_digest = "classification";
  first.preview.bindings.protection_digest = "protection";
  first.preview.bindings.room_plan_digest = "plan";
  first.preview.bindings.config_digest = "config";
  first.preview.bindings.candidate_order_digest = "candidates";
  first.preview.bindings.save_identity = "save";
  first.preview.bindings.game_build_identity = "build";
  const auto stored = store.Store(first);
  AC_CHECK(static_cast<bool>(stored));
  AC_CHECK(stored.value.find("preview-") == 0);
  AC_CHECK(static_cast<bool>(store.Read(stored.value)));

  const auto claimed = store.Claim(stored.value, first.preview.bindings);
  AC_CHECK(static_cast<bool>(claimed));
  AC_CHECK(
      !static_cast<bool>(store.Claim(stored.value, first.preview.bindings)));
  AC_CHECK(!static_cast<bool>(store.Cancel(stored.value)));

  workflow::PreviewBundle changed = first;
  changed.preview.id.clear();
  const auto changed_id = store.Store(changed);
  auto changed_bindings = changed.preview.bindings;
  ++changed_bindings.scene_generation;
  AC_CHECK(!static_cast<bool>(store.Claim(changed_id.value, changed_bindings)));

  workflow::PreviewBundle cancelled = first;
  cancelled.preview.id.clear();
  const auto cancelled_id = store.Store(cancelled);
  AC_CHECK(static_cast<bool>(store.Cancel(cancelled_id.value)));
  AC_CHECK(!static_cast<bool>(store.Read(cancelled_id.value)));

  workflow::PreviewBundle expired = first;
  expired.preview.id.clear();
  expired.preview.expiration.expires_at = now + 1s;
  const auto expired_id = store.Store(expired);
  now += 2s;
  AC_CHECK(!static_cast<bool>(store.Read(expired_id.value)));

  workflow::PreviewBundle duplicate = first;
  duplicate.preview.id = "fixed-preview";
  AC_CHECK(static_cast<bool>(store.Store(duplicate)));
  AC_CHECK(!static_cast<bool>(store.Store(duplicate)));

  workflow::PreviewBundle invalidated = first;
  invalidated.preview.id = "invalidate-me";
  AC_CHECK(static_cast<bool>(store.Store(invalidated)));
  store.InvalidateAll();
  AC_CHECK(!static_cast<bool>(store.Read("invalidate-me")));
}

} // namespace autocattery::tests
