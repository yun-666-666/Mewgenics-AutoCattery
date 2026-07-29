#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace autocattery::snapshot {

using CatId = std::int64_t;
using RoomId = std::string;

enum class TriState {
    Unknown,
    No,
    Yes
};

enum class LifeStage {
    Unknown,
    Kitten,
    Adult,
    Senior,
    Dead
};

enum class Stat {
    Strength,
    Dexterity,
    Constitution,
    Intelligence,
    Speed,
    Charisma,
    Luck,
    Count
};

constexpr std::size_t kStatCount = static_cast<std::size_t>(Stat::Count);

struct StatBlock {
    std::array<std::optional<std::int32_t>, kStatCount> values{};
};

struct CatSnapshot {
    CatId id{};
    std::string display_name;
    std::string breed_id;
    std::string voice_id;
    std::string stat_type_id;
    std::string class_id;
    StatBlock genetic_stats;
    StatBlock heredity_bonus;
    StatBlock equipment_bonus;
    std::vector<std::string> raw_ability_slots;
    std::optional<std::int64_t> birth_day;
    std::optional<std::int64_t> age_days;
    std::optional<RoomId> room_id;
    bool in_adventure_box{};
    LifeStage life_stage{LifeStage::Unknown};
    TriState available_for_combat{TriState::Unknown};
    TriState available_for_breeding{TriState::Unknown};
    TriState injured{TriState::Unknown};
};

struct RoomSnapshot {
    RoomId id;
    std::vector<CatId> residents;
};

struct CapabilityMatrix {
    bool stable_cat_id{};
    bool read_display_name{};
    bool read_genetic_stats{};
    bool read_heredity_bonus{};
    bool read_equipment_bonus{};
    bool read_raw_ability_slots{};
    bool read_typed_abilities{};
    bool read_class_id{};
    bool read_age{};
    bool read_breeding_eligibility{};
    bool read_relationships{};
    bool read_room_assignments{};
    bool read_room_capacities{};
};

struct HouseSnapshot {
    std::uint64_t snapshot_id{};
    std::uint64_t scene_generation{};
    std::optional<std::int64_t> game_day;
    std::string source_save_name;
    std::vector<CatSnapshot> cats;
    std::vector<RoomSnapshot> rooms;
    CapabilityMatrix capabilities;
    std::chrono::system_clock::time_point captured_at;
};

enum class ValidationSeverity {
    Warning,
    Error
};

struct ValidationIssue {
    ValidationSeverity severity{ValidationSeverity::Warning};
    std::string code;
};

struct SnapshotValidation {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] std::size_t WarningCount() const noexcept;
    [[nodiscard]] std::size_t ErrorCount() const noexcept;
};

SnapshotValidation Validate(const HouseSnapshot& snapshot);

}  // namespace autocattery::snapshot
