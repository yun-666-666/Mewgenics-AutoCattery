#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "auto_cattery/error.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"

namespace autocattery::workflow {

class PreviewStore final {
public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;

  explicit PreviewStore(Clock clock = {});
  [[nodiscard]] Result<PreviewId> Store(PreviewBundle bundle);
  [[nodiscard]] Result<PreviewBundle> Read(const PreviewId &id) const;
  [[nodiscard]] Result<PreviewBundle>
  Claim(const PreviewId &id, const PreviewBindings &current_bindings);
  [[nodiscard]] Result<void> Cancel(const PreviewId &id);

private:
  struct Entry {
    PreviewBundle bundle;
    bool cancelled{};
    bool consumed{};
  };

  Clock clock_;
  mutable std::mutex mutex_;
  std::unordered_map<PreviewId, Entry> entries_;
  std::uint64_t sequence_{};
};

} // namespace autocattery::workflow
