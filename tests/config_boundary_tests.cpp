#include "auto_cattery/config.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::filesystem::path BoundaryDirectory() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        "auto_cattery_stage13_config_boundary_tests";
    std::filesystem::create_directories(directory);
    return directory;
}

void WriteBoundary(
    const std::filesystem::path& path,
    const std::string& contents) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

Result<Config> LoadUserText(const std::string& contents) {
    const auto user = BoundaryDirectory() / "user.json";
    WriteBoundary(user, contents);
    return LoadConfig({}, user);
}

}  // namespace

void RunConfigBoundaryTests() {
    const auto missing = LoadConfig({}, {});
    AC_CHECK(static_cast<bool>(missing));
    AC_CHECK(missing.value.execution_safety.read_only_mode);
    AC_CHECK(missing.value.execution_safety.create_backup_before_apply);
    AC_CHECK(missing.value.recommendation_marker.never_auto_select);

    for (const auto count : {1, 100}) {
        const auto result = LoadUserText(
            "{\"combat_scoring\":{\"recommended_count\":" +
            std::to_string(count) + "}}");
        AC_CHECK(static_cast<bool>(result));
        AC_CHECK(result.value.combat_scoring.recommended_count ==
                 static_cast<std::size_t>(count));
    }
    for (const auto count : {0, 101}) {
        const auto result = LoadUserText(
            "{\"combat_scoring\":{\"recommended_count\":" +
            std::to_string(count) + "}}");
        AC_CHECK(!static_cast<bool>(result));
        AC_CHECK(result.message.find("recommended_count") != std::string::npos);
    }

    const auto finite_edges = LoadUserText(R"({
        "combat_scoring": {
            "stat_weights": {"strength": -10000, "luck": 10000}
        }
    })");
    AC_CHECK(static_cast<bool>(finite_edges));
    AC_CHECK(finite_edges.value.combat_scoring.stat_weights[0] == -10000.0);
    AC_CHECK(finite_edges.value.combat_scoring.stat_weights[6] == 10000.0);

    const auto excessive_weight = LoadUserText(R"({
        "combat_scoring": {"stat_weights": {"strength": 10000.1}}
    })");
    AC_CHECK(!static_cast<bool>(excessive_weight));
    AC_CHECK(excessive_weight.message.find("strength") != std::string::npos);

    const auto unknown_stat = LoadUserText(R"({
        "combat_scoring": {"stat_weights": {"unknown_stat": 1}}
    })");
    AC_CHECK(!static_cast<bool>(unknown_stat));
    AC_CHECK(unknown_stat.message.find("unknown_stat") != std::string::npos);

    const auto unknown_enum =
        LoadUserText(R"({"general":{"log_level":"verbose"}})");
    AC_CHECK(!static_cast<bool>(unknown_enum));
    AC_CHECK(unknown_enum.message.find("log_level") != std::string::npos);

    const auto nan_string = LoadUserText(
        R"({"combat_scoring":{"minimum_score":"NaN"}})");
    AC_CHECK(!static_cast<bool>(nan_string));

    const auto negative_pool = LoadUserText(
        R"({"classification":{"minimum_general_reserve":-1}})");
    AC_CHECK(!static_cast<bool>(negative_pool));

    const auto negative_overflow = LoadUserText(
        R"({"room_planning":{"max_soft_overflow_per_room":-1}})");
    AC_CHECK(!static_cast<bool>(negative_overflow));

    const auto unknown_array = LoadUserText(
        R"({"unknown_future_field":[1,2,3]})");
    AC_CHECK(!static_cast<bool>(unknown_array));
    AC_CHECK(unknown_array.message.find("arrays") != std::string::npos);

    const auto root_array = LoadUserText("[]");
    AC_CHECK(!static_cast<bool>(root_array));
    AC_CHECK(root_array.message.find("root") != std::string::npos);

    const auto preview_disabled = LoadUserText(R"({
        "execution_safety": {
            "require_preview_before_destructive_actions": false
        }
    })");
    AC_CHECK(!static_cast<bool>(preview_disabled));

    const auto build_guard_disabled = LoadUserText(R"({
        "execution_safety": {"abort_on_unknown_game_build": false}
    })");
    AC_CHECK(!static_cast<bool>(build_guard_disabled));

    const auto auto_select = LoadUserText(R"({
        "recommendation_marker": {"never_auto_select": false}
    })");
    AC_CHECK(!static_cast<bool>(auto_select));
    AC_CHECK(auto_select.message.find("never_auto_select") != std::string::npos);

    const auto unverified_pulse = LoadUserText(R"({
        "recommendation_marker": {"pulse_top_n": 1}
    })");
    AC_CHECK(!static_cast<bool>(unverified_pulse));

    const auto stale_marker = LoadUserText(R"({
        "recommendation_marker": {"recompute_if_stale": false}
    })");
    AC_CHECK(!static_cast<bool>(stale_marker));

    const auto no_backup_with_cull = LoadUserText(R"({
        "execution_safety": {"create_backup_before_apply": false},
        "execution": {"cull_enabled": true}
    })");
    AC_CHECK(static_cast<bool>(no_backup_with_cull));
    AC_CHECK(!no_backup_with_cull.value.execution.cull_enabled);
    AC_CHECK(!no_backup_with_cull.value.execution_safety.create_backup_before_apply);

    const auto unverified_write = LoadUserText(R"({
        "execution": {"real_write_adapter_enabled": true}
    })");
    AC_CHECK(!static_cast<bool>(unverified_write));

    const auto future = LoadUserText(R"({
        "schema_version": 99,
        "execution_safety": {"read_only_mode": false}
    })");
    AC_CHECK(static_cast<bool>(future));
    AC_CHECK(future.value.force_read_only);
    AC_CHECK(future.value.execution_safety.read_only_mode);

    const auto oversized_path = BoundaryDirectory() / "oversized.json";
    WriteBoundary(
        oversized_path,
        "{\"padding\":\"" + std::string(1024U * 1024U, 'x') + "\"}");
    const auto oversized = LoadConfig({}, oversized_path);
    AC_CHECK(!static_cast<bool>(oversized));
    AC_CHECK(oversized.message.find("1 MiB") != std::string::npos);
}

}  // namespace autocattery::tests
