#include "auto_cattery/workflow/preview_store.hpp"

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

workflow::PreviewBundle
BoundPreview(std::chrono::steady_clock::time_point now) {
  workflow::PreviewBundle bundle;
  bundle.preview.expiration.expires_at = now + std::chrono::minutes(2);
  bundle.preview.bindings = {
      5,           10,       "snapshot",        "classification", "protection",
      "room-plan", "config", "candidate-order", "save",           "build"};
  return bundle;
}

} // namespace

void RunWorkflowPreviewBindingTests() {
  const auto now = std::chrono::steady_clock::now();
  workflow::PreviewStore store([now] { return now; });
  using Mutation = std::function<void(workflow::PreviewBindings &)>;
  const std::vector<Mutation> mutations{
      [](auto &value) { ++value.scene_generation; },
      [](auto &value) { value.game_day = 11; },
      [](auto &value) { value.snapshot_content_digest += "-changed"; },
      [](auto &value) { value.classification_digest += "-changed"; },
      [](auto &value) { value.protection_digest += "-changed"; },
      [](auto &value) { value.room_plan_digest += "-changed"; },
      [](auto &value) { value.config_digest += "-changed"; },
      [](auto &value) { value.candidate_order_digest += "-changed"; },
      [](auto &value) { value.save_identity += "-changed"; },
      [](auto &value) { value.game_build_identity += "-changed"; }};
  for (const auto &mutate : mutations) {
    auto bundle = BoundPreview(now);
    const auto expected = bundle.preview.bindings;
    const auto id = store.Store(std::move(bundle));
    AC_CHECK(static_cast<bool>(id));
    auto changed = expected;
    mutate(changed);
    AC_CHECK(!static_cast<bool>(store.Claim(id.value, changed)));
    AC_CHECK(static_cast<bool>(store.Claim(id.value, expected)));
  }

  auto concurrent = BoundPreview(now);
  const auto bindings = concurrent.preview.bindings;
  const auto id = store.Store(std::move(concurrent));
  std::atomic<int> successes{};
  const auto claim = [&] {
    if (store.Claim(id.value, bindings)) {
      ++successes;
    }
  };
  std::thread first(claim);
  std::thread second(claim);
  first.join();
  second.join();
  AC_CHECK(successes == 1);
}

} // namespace autocattery::tests
