#include "auto_cattery/recommendation/snapshot_reader.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "test_support.hpp"

namespace autocattery::tests {

void RunRecommendationSnapshotReaderTests() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("auto_cattery_phase12_reader_" + std::to_string(nonce));
    const auto path = root / "recommendations.json";
    AC_CHECK(
        recommendation::RecommendationSidecarPath(root) ==
        root / "state" / "recommendations.json");
    AC_CHECK(
        recommendation::ReadRecommendationSnapshot(path).status ==
        recommendation::SnapshotReadStatus::Missing);

    std::filesystem::create_directories(root);
    nlohmann::json payload{
        {"schema_version", 1},
        {"created_on_game_day", 12},
        {"source_snapshot_id", 44},
        {"combat_algorithm_version", "fixture-v1"},
        {"config_digest", "config-digest"},
        {"recommended", {{{"cat_id", "101"}, {"rank", 1},
                           {"score", 91.5}, {"confidence", 1.0}}}}
    };
    auto Write = [&](const nlohmann::json& value, std::string checksum) {
        std::ofstream output(path, std::ios::trunc);
        output << nlohmann::json{
            {"checksum", std::move(checksum)}, {"payload", value}}.dump(2);
    };
    Write(
        payload,
        workflow::RecommendationPayloadChecksum(payload.dump()));
    const auto read = recommendation::ReadRecommendationSnapshot(path);
    AC_CHECK(read.status == recommendation::SnapshotReadStatus::Available);
    AC_CHECK(read.snapshot.recommended.size() == 1);
    AC_CHECK(read.snapshot.recommended.front().cat_id == 101);

    Write(payload, "0000000000000000");
    AC_CHECK(
        recommendation::ReadRecommendationSnapshot(path).status ==
        recommendation::SnapshotReadStatus::Rejected);
    payload["schema_version"] = 0;
    Write(
        payload,
        workflow::RecommendationPayloadChecksum(payload.dump()));
    AC_CHECK(
        recommendation::ReadRecommendationSnapshot(path).status ==
        recommendation::SnapshotReadStatus::Rejected);
    payload["schema_version"] = 2;
    Write(
        payload,
        workflow::RecommendationPayloadChecksum(payload.dump()));
    AC_CHECK(
        recommendation::ReadRecommendationSnapshot(path).status ==
        recommendation::SnapshotReadStatus::Rejected);
    payload.erase("config_digest");
    Write(
        payload,
        workflow::RecommendationPayloadChecksum(payload.dump()));
    AC_CHECK(
        recommendation::ReadRecommendationSnapshot(path).status ==
        recommendation::SnapshotReadStatus::Rejected);

    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    AC_CHECK(!cleanup);
}

}  // namespace autocattery::tests
