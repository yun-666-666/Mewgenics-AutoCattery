#include "auto_cattery/recommendation/snapshot_reader.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace autocattery::recommendation {
namespace {

SnapshotReadResult Rejected(std::string reason) {
    return {SnapshotReadStatus::Rejected, {}, std::move(reason)};
}

bool ParseCatId(std::string_view text, snapshot::CatId& value) {
    if (text.empty()) {
        return false;
    }
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} &&
           parsed.ptr == text.data() + text.size() &&
           value > 0;
}

}  // namespace

std::filesystem::path RecommendationSidecarPath(
    const std::filesystem::path& mod_root) {
    return mod_root / L"state" / L"recommendations.json";
}

SnapshotReadResult ReadRecommendationSnapshot(
    const std::filesystem::path& sidecar_path) {
    if (sidecar_path.empty() || !std::filesystem::exists(sidecar_path)) {
        return {
            SnapshotReadStatus::Missing,
            {},
            "recommendation sidecar is absent"
        };
    }

    try {
        std::ifstream input(sidecar_path, std::ios::binary);
        nlohmann::json envelope;
        input >> envelope;
        if (!input || !envelope.is_object() ||
            !envelope.contains("checksum") ||
            !envelope.at("checksum").is_string() ||
            !envelope.contains("payload") ||
            !envelope.at("payload").is_object()) {
            return Rejected("recommendation envelope is malformed");
        }

        const auto& payload = envelope.at("payload");
        if (!payload.contains("schema_version") ||
            !payload.at("schema_version").is_number_integer() ||
            payload.at("schema_version").get<int>() != 1) {
            return Rejected("recommendation schema is unsupported");
        }
        if (workflow::RecommendationPayloadChecksum(payload.dump()) !=
            envelope.at("checksum").get<std::string>()) {
            return Rejected("recommendation checksum mismatch");
        }

        workflow::RecommendationSnapshot result;
        result.game_day = payload.at("created_on_game_day").get<std::int64_t>();
        result.source_snapshot_id =
            payload.at("source_snapshot_id").get<std::uint64_t>();
        result.combat_algorithm_version =
            payload.at("combat_algorithm_version").get<std::string>();
        result.config_digest = payload.at("config_digest").get<std::string>();
        const auto& entries = payload.at("recommended");
        if (result.source_snapshot_id == 0 ||
            result.combat_algorithm_version.empty() ||
            result.config_digest.empty() ||
            !entries.is_array()) {
            return Rejected("recommendation payload fields are invalid");
        }

        std::unordered_set<snapshot::CatId> ids;
        std::unordered_set<std::size_t> ranks;
        for (const auto& item : entries) {
            workflow::RecommendationEntry entry;
            if (!item.is_object() ||
                !item.at("cat_id").is_string() ||
                !ParseCatId(item.at("cat_id").get_ref<const std::string&>(),
                            entry.cat_id)) {
                return Rejected("recommendation CatId is invalid");
            }
            entry.rank = item.at("rank").get<std::size_t>();
            entry.score = item.at("score").get<double>();
            entry.confidence = item.at("confidence").get<double>();
            if (entry.rank == 0 || !std::isfinite(entry.score) ||
                !std::isfinite(entry.confidence) ||
                entry.confidence < 0.0 || entry.confidence > 1.0 ||
                !ids.insert(entry.cat_id).second ||
                !ranks.insert(entry.rank).second) {
                return Rejected("recommendation entry is invalid");
            }
            result.recommended.push_back(entry);
        }
        return {SnapshotReadStatus::Available, std::move(result), {}};
    } catch (const nlohmann::json::exception&) {
        return Rejected("recommendation JSON fields are missing or invalid");
    } catch (const std::exception&) {
        return Rejected("recommendation sidecar could not be read");
    }
}

}  // namespace autocattery::recommendation
