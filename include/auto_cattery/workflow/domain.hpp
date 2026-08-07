#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::workflow {

using PreviewId = std::string;

enum class WorkflowState {
  Idle,
  Capturing,
  Scoring,
  Planning,
  AwaitingConfirmation,
  Applying,
  Verifying,
  Completed,
  Failed,
  Cancelled
};

enum class WorkflowCapability { PreviewOnly, MoveOnly, MoveAndCull };

enum class WorkflowFailureReason {
  None,
  Busy,
  CaptureFailed,
  AnalysisFailed,
  InvalidTransition,
  NotAvailable,
  Cancelled,
  Expired,
  AlreadyConsumed,
  PreconditionsChanged,
  ExecutionFailed
};

enum class ExecutionChoice { Execute, MoveOnly };

struct PreviewExpiration {
  std::chrono::steady_clock::time_point expires_at;
};

struct PreviewBindings {
  std::uint64_t scene_generation{};
  std::optional<std::int64_t> game_day;
  std::string snapshot_content_digest;
  std::string classification_digest;
  std::string protection_digest;
  std::string room_plan_digest;
  std::string config_digest;
  std::string candidate_order_digest;
  std::string save_identity;
  std::string game_build_identity;

  bool operator==(const PreviewBindings &) const = default;
};

struct OrganizePreview {
  PreviewId id;
  WorkflowCapability capability{WorkflowCapability::PreviewOnly};
  PreviewExpiration expiration;
  PreviewBindings bindings;
  std::size_t cat_count{};
  std::size_t room_count{};
  std::size_t recommended_combat_count{};
  std::size_t breeding_core_count{};
  std::size_t breeding_reserve_count{};
  std::size_t protected_count{};
  std::size_t planned_move_count{};
  std::size_t capacity_relief_count{};
  std::size_t unplaced_count{};
  room_planning::PlanDisposition disposition{
      room_planning::PlanDisposition::Invalid};
  bool fully_satisfied{};
  bool game_data_modified{};
  std::vector<std::string> warnings;
  std::string summary;
};

struct OrganizeOutcome {
  PreviewId preview_id;
  WorkflowCapability capability{WorkflowCapability::PreviewOnly};
  WorkflowState state{WorkflowState::Failed};
  WorkflowFailureReason failure_reason{WorkflowFailureReason::None};
  std::size_t completed_moves{};
  std::size_t remaining_moves{};
  std::size_t completed_culls{};
  bool game_data_modified{};
  bool recommendation_snapshot_available{};
  std::string message;
};

} // namespace autocattery::workflow
