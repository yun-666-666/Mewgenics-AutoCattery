#include "auto_cattery/settings_file_editor.hpp"

#include <array>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include <windows.h>

namespace autocattery {
namespace {

using Json = nlohmann::json;
constexpr std::uintmax_t kMaximumUserConfigBytes = 1024U * 1024U;

Result<Json> ReadUserLayer(const std::filesystem::path& path) {
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error)) {
        return {Json::object()};
    }
    if (error) {
        return {{}, ErrorCode::ConfigInvalid, "cannot inspect user_config.json"};
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > kMaximumUserConfigBytes) {
        return {{}, ErrorCode::ConfigInvalid, "user_config.json exceeds 1 MiB"};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {{}, ErrorCode::ConfigInvalid, "cannot open user_config.json"};
    }
    try {
        Json value;
        stream >> value;
        if (!value.is_object()) {
            return {{}, ErrorCode::ConfigInvalid,
                    "user_config.json root must be an object"};
        }
        return {std::move(value)};
    } catch (const Json::exception& exception) {
        return {{}, ErrorCode::ConfigInvalid, exception.what()};
    }
}

void SetStatWeights(
    Json& layer,
    const char* module,
    const std::array<double, snapshot::kStatCount>& weights) {
    static constexpr std::array<const char*, snapshot::kStatCount> keys{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    for (std::size_t index = 0; index < keys.size(); ++index) {
        layer[module]["stat_weights"][keys[index]] = weights[index];
    }
}

Json EditableLayer(const Json& existing, const Config& config) {
    Json layer = existing;
    layer["general"]["language"] = config.general.language;
    auto& combat = layer["combat_scoring"];
    combat["recommended_count"] = config.combat_scoring.recommended_count;
    combat["minimum_score"] = config.combat_scoring.minimum_score;
    combat["minimum_known_stats"] = config.combat_scoring.minimum_known_stats;
    combat["exclude_kittens"] = config.combat_scoring.exclude_kittens;
    combat["exclude_injured"] = config.combat_scoring.exclude_injured;
    combat["require_confirmed_eligibility"] =
        config.combat_scoring.require_confirmed_eligibility;
    combat["missing_stat_penalty"] =
        config.combat_scoring.missing_stat_penalty;
    combat["injury_penalty"] = config.combat_scoring.injury_penalty;

    auto& breeding = layer["breeding_scoring"];
    breeding["core_breeders"] = config.breeding_scoring.core_breeders;
    breeding["reserve_breeders"] = config.breeding_scoring.reserve_breeders;
    breeding["minimum_score"] = config.breeding_scoring.minimum_score;
    breeding["minimum_known_stats"] =
        config.breeding_scoring.minimum_known_stats;
    breeding["require_confirmed_eligibility"] =
        config.breeding_scoring.require_confirmed_eligibility;
    breeding["missing_stat_penalty"] =
        config.breeding_scoring.missing_stat_penalty;

    layer["recommendation_marker"]["recommended_count"] =
        config.recommendation_marker.recommended_count;
    layer["recommendation_marker"]["show_score"] =
        config.recommendation_marker.show_score;
    layer["recommendation_marker"]["show_rank"] =
        config.recommendation_marker.show_rank;
    SetStatWeights(
        layer,
        "combat_scoring",
        config.combat_scoring.stat_weights);
    SetStatWeights(
        layer,
        "breeding_scoring",
        config.breeding_scoring.stat_weights);
    auto& classification = layer["classification"];
    classification["combat_priority_over_breeding"] =
        config.classification.combat_priority_over_breeding;
    classification["minimum_combat_pool"] =
        config.classification.minimum_combat_pool;
    classification["minimum_breeding_pool"] =
        config.classification.minimum_breeding_pool;
    classification["minimum_general_reserve"] =
        config.classification.minimum_general_reserve;
    classification["never_cull_if_data_confidence_below"] =
        config.classification.never_cull_if_data_confidence_below;

    auto& planning = layer["room_planning"];
    planning["breeding_room_population"] = config.room_planning.breeding_room_population;
    planning["default_soft_capacity"] =
        config.room_planning.default_soft_capacity;
    planning["allow_soft_overflow"] =
        config.room_planning.allow_soft_overflow;
    planning["max_soft_overflow_per_room"] =
        config.room_planning.max_soft_overflow_per_room;
    planning["prefer_single_combat_staging_room"] =
        config.room_planning.prefer_single_combat_staging_room;
    planning["keep_breeding_pairs_together"] =
        config.room_planning.keep_breeding_pairs_together;
    planning["avoid_inbreeding_pairs"] =
        config.room_planning.avoid_inbreeding_pairs;
    planning["keep_kittens_separate_when_possible"] =
        config.room_planning.keep_kittens_separate_when_possible;
    layer["execution_safety"]["read_only_mode"] =
        config.execution_safety.read_only_mode;
    layer["execution_safety"]["create_backup_before_apply"] =
        config.execution_safety.create_backup_before_apply;
    layer["execution_safety"]["single_click_execute"] =
        config.execution_safety.single_click_execute;
    layer["diagnostics"]["collect_cat_data"] =
        config.diagnostics.collect_cat_data;
    layer["level_up"]["reroll_count"] = config.level_up.reroll_count;
    return layer;
}

Result<void> WriteText(
    const std::filesystem::path& path,
    const std::string& text) {
    if (path.empty()) {
        return {ErrorCode::ConfigInvalid, "user_config.json path is empty"};
    }
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return {ErrorCode::ConfigInvalid,
                    "cannot create the configuration directory"};
        }
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return {ErrorCode::ConfigInvalid,
                "cannot create temporary user_config.json"};
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream) {
        return {ErrorCode::ConfigInvalid,
                "cannot write temporary user_config.json"};
    }
    return {};
}

Result<void> ReplaceAtomic(
    const std::filesystem::path& temporary,
    const std::filesystem::path& path) {
    if (MoveFileExW(
            temporary.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return {ErrorCode::ConfigInvalid,
                "cannot atomically replace user_config.json"};
    }
    return {};
}

}  // namespace

SettingsFileEditor::SettingsFileEditor(SettingsFilePaths paths)
    : paths_(std::move(paths)) {}

Result<Config> SettingsFileEditor::Load() const {
    return LoadConfig(paths_.default_config, paths_.user_config);
}

Result<Config> SettingsFileEditor::Save(const Config& config) const {
    const auto existing = ReadUserLayer(paths_.user_config);
    if (!existing) {
        return {{}, existing.code, existing.message};
    }
    const auto layer = EditableLayer(existing.value, config);
    auto temporary = paths_.user_config;
    temporary += ".candidate.tmp";
    const auto serialized = layer.dump(2) + "\n";
    std::error_code error;
    std::filesystem::remove(temporary, error);
    const auto written = WriteText(temporary, serialized);
    if (!written) {
        return {{}, written.code, written.message};
    }
    const auto loaded = LoadConfig(paths_.default_config, temporary);
    if (!loaded) {
        std::filesystem::remove(temporary, error);
        return {{}, loaded.code,
                "saved configuration failed validation: " + loaded.message};
    }
    const auto replaced = ReplaceAtomic(temporary, paths_.user_config);
    if (!replaced) {
        return {{}, replaced.code, replaced.message};
    }
    return loaded;
}

}  // namespace autocattery
