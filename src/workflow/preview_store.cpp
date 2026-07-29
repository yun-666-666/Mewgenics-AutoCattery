#include "auto_cattery/workflow/preview_store.hpp"

#include <iomanip>
#include <sstream>

namespace autocattery::workflow {
namespace {

PreviewId MakeId(const PreviewBindings &bindings, std::uint64_t sequence) {
  std::ostringstream canonical;
  canonical << bindings.scene_generation << '|'
            << bindings.game_day.value_or(-1) << '|'
            << bindings.snapshot_content_digest << '|'
            << bindings.classification_digest << '|'
            << bindings.protection_digest << '|' << bindings.room_plan_digest
            << '|' << bindings.config_digest << '|'
            << bindings.candidate_order_digest << '|' << bindings.save_identity
            << '|' << bindings.game_build_identity << '|' << sequence;
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : canonical.str()) {
    hash = (hash ^ byte) * 1099511628211ULL;
  }
  std::ostringstream id;
  id << "preview-" << std::hex << std::setfill('0') << std::setw(16) << hash;
  return id.str();
}

} // namespace

PreviewStore::PreviewStore(Clock clock) : clock_(std::move(clock)) {
  if (!clock_) {
    clock_ = [] { return std::chrono::steady_clock::now(); };
  }
}

Result<PreviewId> PreviewStore::Store(PreviewBundle bundle) {
  std::scoped_lock lock(mutex_);
  if (bundle.preview.id.empty()) {
    bundle.preview.id = MakeId(bundle.preview.bindings, ++sequence_);
  }
  if (entries_.contains(bundle.preview.id)) {
    return {{}, ErrorCode::WriteConflict, "duplicate preview ID"};
  }
  const auto id = bundle.preview.id;
  entries_.emplace(id, Entry{std::move(bundle)});
  return {id};
}

Result<PreviewBundle> PreviewStore::Read(const PreviewId &id) const {
  std::scoped_lock lock(mutex_);
  const auto entry = entries_.find(id);
  if (entry == entries_.end()) {
    return {{}, ErrorCode::OperationCancelled, "preview not found"};
  }
  if (entry->second.cancelled) {
    return {{}, ErrorCode::OperationCancelled, "preview cancelled"};
  }
  if (clock_() >= entry->second.bundle.preview.expiration.expires_at) {
    return {{}, ErrorCode::OperationCancelled, "preview expired"};
  }
  return {entry->second.bundle};
}

Result<PreviewBundle>
PreviewStore::Claim(const PreviewId &id,
                    const PreviewBindings &current_bindings) {
  std::scoped_lock lock(mutex_);
  const auto entry = entries_.find(id);
  if (entry == entries_.end() || entry->second.cancelled) {
    return {{}, ErrorCode::OperationCancelled, "preview unavailable"};
  }
  if (entry->second.consumed) {
    return {{}, ErrorCode::WriteConflict, "preview already consumed"};
  }
  if (clock_() >= entry->second.bundle.preview.expiration.expires_at) {
    return {{}, ErrorCode::OperationCancelled, "preview expired"};
  }
  if (entry->second.bundle.preview.bindings != current_bindings) {
    return {{},
            ErrorCode::WriteConflict,
            "preview bindings changed; create a new preview"};
  }
  entry->second.consumed = true;
  return {entry->second.bundle};
}

Result<void> PreviewStore::Cancel(const PreviewId &id) {
  std::scoped_lock lock(mutex_);
  const auto entry = entries_.find(id);
  if (entry == entries_.end()) {
    return {ErrorCode::OperationCancelled, "preview not found"};
  }
  if (entry->second.consumed) {
    return {ErrorCode::WriteConflict, "preview already consumed"};
  }
  entry->second.cancelled = true;
  return {};
}

void PreviewStore::InvalidateAll() noexcept {
  std::scoped_lock lock(mutex_);
  entries_.clear();
}

} // namespace autocattery::workflow
