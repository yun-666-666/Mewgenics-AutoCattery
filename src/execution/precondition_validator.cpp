#include "auto_cattery/execution/precondition_validator.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/execution/plan_sealer.hpp"
#include "auto_cattery/protection/policy.hpp"

namespace autocattery::execution {
namespace {

ApprovalResult Reject(
    ApprovalDisposition disposition,
    std::string reason) {
    return {.disposition = disposition, .reason = std::move(reason)};
}

bool CanMove(const protection::ProtectionDecision& decision) {
    return decision.automatically_managed && decision.move_allowed &&
           !decision.fail_closed &&
           (decision.effective_level == protection::ProtectionLevel::None ||
            decision.effective_level == protection::ProtectionLevel::NoCull);
}

bool CanCull(const protection::ProtectionDecision& decision) {
    return decision.automatically_managed && decision.cull_allowed &&
           !decision.fail_closed &&
           (decision.effective_level == protection::ProtectionLevel::None ||
            decision.effective_level == protection::ProtectionLevel::NoMove);
}

}  // namespace

ApprovalResult PreconditionValidator::Validate(
    const ApprovalRequest& request) const {
    if (request.operation_id.empty() || request.save_identity.empty() ||
        request.game_build_identity.empty()) {
        return Reject(
            ApprovalDisposition::Unsupported,
            "stable operation, save, and build identities are required");
    }
    if (request.expected_scene_generation == 0 ||
        request.expected_scene_generation !=
            request.current_scene_generation ||
        request.snapshot.scene_generation !=
            request.current_scene_generation ||
        request.snapshot.game_day != request.current_game_day) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "scene generation or game day changed");
    }
    if (request.snapshot.snapshot_id == 0 ||
        !snapshot::Validate(request.snapshot).Valid() ||
        !request.snapshot.capabilities.stable_cat_id ||
        !request.snapshot.capabilities.read_room_assignments) {
        return Reject(
            ApprovalDisposition::Unsupported,
            "snapshot identity or room assignments are unsupported");
    }
    if (request.classification.source_snapshot_id !=
            request.snapshot.snapshot_id ||
        request.room_plan.source_snapshot_id !=
            request.snapshot.snapshot_id) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "preview inputs came from different captures");
    }
    if (request.room_plan.disposition ==
            room_planning::PlanDisposition::Invalid ||
        request.room_plan.disposition ==
            room_planning::PlanDisposition::CancelAndRepreview) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "room plan is no longer approvable");
    }
    if (protection::Recheck(
            request.preview_protection_digest,
            request.current_protection_digest) !=
            protection::RecheckResult::Unchanged ||
        protection::BuildDigest(request.protections) !=
            request.current_protection_digest) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "protection digest changed");
    }

    std::unordered_map<snapshot::CatId, const snapshot::CatSnapshot*> cats;
    std::unordered_set<snapshot::RoomId> rooms;
    for (const auto& cat : request.snapshot.cats) {
        if (!cats.emplace(cat.id, &cat).second) {
            return Reject(
                ApprovalDisposition::CancelAndRepreview,
                "duplicate cat identity");
        }
    }
    for (const auto& room : request.snapshot.rooms) {
        if (!rooms.insert(room.id).second) {
            return Reject(
                ApprovalDisposition::CancelAndRepreview,
                "duplicate room identity");
        }
    }

    std::unordered_map<
        snapshot::CatId,
        const protection::ProtectionDecision*> protections;
    for (const auto& protection : request.protections) {
        if (!cats.contains(protection.cat_id) ||
            !protections.emplace(protection.cat_id, &protection).second) {
            return Reject(
                ApprovalDisposition::CancelAndRepreview,
                "unknown or duplicate protection identity");
        }
    }
    if (protections.size() != cats.size()) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "protection decisions are incomplete");
    }

    std::unordered_set<snapshot::CatId> classified;
    for (const auto& decision : request.classification.decisions) {
        if (!cats.contains(decision.cat_id) ||
            !classified.insert(decision.cat_id).second) {
            return Reject(
                ApprovalDisposition::CancelAndRepreview,
                "unknown or duplicate classification identity");
        }
    }
    if (classified.size() != cats.size()) {
        return Reject(
            ApprovalDisposition::CancelAndRepreview,
            "classification decisions are incomplete");
    }

    std::vector<ApprovedMove> moves;
    std::unordered_set<snapshot::CatId> action_ids;
    for (const auto& move : request.room_plan.moves) {
        const auto cat = cats.find(move.cat_id);
        const auto policy = protections.find(move.cat_id);
        if (cat == cats.end() || policy == protections.end() ||
            cat->second->in_adventure_box ||
            !cat->second->room_id.has_value() ||
            *cat->second->room_id != move.from_room ||
            move.from_room == move.to_room ||
            !rooms.contains(move.from_room) ||
            !rooms.contains(move.to_room) ||
            !CanMove(*policy->second) ||
            !action_ids.insert(move.cat_id).second) {
            return Reject(
                ApprovalDisposition::CancelAndRepreview,
                "move set changed or violates protection");
        }
        moves.push_back({move.cat_id, move.from_room, move.to_room});
    }

    std::vector<ApprovedCull> culls;
    if (request.approve_culls) {
        for (const auto& suggestion :
             request.room_plan.capacity_relief_suggestions) {
            const auto cat = cats.find(suggestion.cat_id);
            const auto policy = protections.find(suggestion.cat_id);
            const auto candidate = std::ranges::find(
                request.classification.capacity_relief_candidates,
                suggestion.cat_id);
            if (cat == cats.end() || policy == protections.end() ||
                candidate ==
                    request.classification.capacity_relief_candidates.end() ||
                static_cast<std::size_t>(
                    candidate -
                    request.classification
                        .capacity_relief_candidates.begin()) !=
                    suggestion.candidate_order ||
                cat->second->in_adventure_box ||
                !CanCull(*policy->second) ||
                !action_ids.insert(suggestion.cat_id).second) {
                return Reject(
                    ApprovalDisposition::CancelAndRepreview,
                    "cull set changed or violates protection");
            }
            culls.push_back(
                {suggestion.cat_id, suggestion.candidate_order});
        }
        if (culls.size() <
            request.room_plan.minimum_capacity_relief_required) {
            return Reject(
                ApprovalDisposition::Unsupported,
                "approved cull suggestions cannot satisfy capacity");
        }
    }

    OperationPrecondition precondition{
        .snapshot_id = request.snapshot.snapshot_id,
        .scene_generation = request.current_scene_generation,
        .game_day = request.current_game_day,
        .game_build_identity = request.game_build_identity,
        .save_identity = request.save_identity,
        .snapshot_content_digest =
            DigestSnapshotContent(request.snapshot),
        .classification_digest =
            DigestClassification(request.classification),
        .plan_digest = DigestRoomPlan(request.room_plan),
        .protection_digest =
            request.current_protection_digest.value
    };
    ApprovedExecutionPlan plan(
        request.operation_id, precondition, std::move(moves),
        std::move(culls));
    ExecutionAuthorization authorization(
        request.operation_id,
        precondition.plan_digest,
        BuildAuthorizationSeal(request.operation_id, precondition));
    return {
        .disposition = ApprovalDisposition::Approved,
        .plan = std::move(plan),
        .authorization = std::move(authorization)
    };
}

bool AuthorizationMatches(
    const ApprovedExecutionPlan& plan,
    const ExecutionAuthorization& authorization) noexcept {
    return plan.Operation() == authorization.Operation() &&
           plan.Precondition().plan_digest ==
               authorization.PlanDigest() &&
           BuildAuthorizationSeal(
               plan.Operation(), plan.Precondition()) ==
               authorization.Seal();
}

}  // namespace autocattery::execution
