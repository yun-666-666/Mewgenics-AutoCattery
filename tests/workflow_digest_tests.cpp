#include "auto_cattery/workflow/digests.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWorkflowDigestTests() {
  Config first;
  Config same = first;
  first.combat_scoring.active_ability_overrides = {{"ability-b", 2.0},
                                                   {"ability-a", 1.0}};
  same.combat_scoring.active_ability_overrides = {{"ability-a", 1.0},
                                                  {"ability-b", 2.0}};
  AC_CHECK(workflow::DigestConfig(first) == workflow::DigestConfig(same));
  same.combat_scoring.stat_weights[5] = 2.0;
  AC_CHECK(workflow::DigestConfig(first) != workflow::DigestConfig(same));
  same = first;
  same.execution.require_quiescent_backup = false;
  AC_CHECK(workflow::DigestConfig(first) != workflow::DigestConfig(same));
  same = first;
  same.protection.sidecar_file = "other.json";
  AC_CHECK(workflow::DigestConfig(first) != workflow::DigestConfig(same));

  classification::ClassificationPlan ordered;
  ordered.capacity_relief_candidates = {1, 2};
  auto reordered = ordered;
  reordered.capacity_relief_candidates = {2, 1};
  AC_CHECK(workflow::DigestCandidateOrder(ordered) !=
           workflow::DigestCandidateOrder(reordered));
  AC_CHECK(workflow::DigestPrivateIdentity("private-save") != "private-save");
}

} // namespace autocattery::tests
