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

enum class CatSex {
    Unknown,
    Female,
    Male
};

enum class CatSexuality {
    Unknown,
    Straight,
    Bisexual,
    Gay
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
    CatSex sex{CatSex::Unknown};
    CatSexuality sexuality{CatSexuality::Unknown};
    std::optional<double> sexuality_coefficient;
    std::optional<CatId> parent_a_id;
    std::optional<CatId> parent_b_id;
    std::optional<double> inbreeding_coefficient;
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

struct PedigreeEntry {
    CatId cat_id{};
    std::optional<CatId> parent_a_id;
    std::optional<CatId> parent_b_id;
    std::optional<double> inbreeding_coefficient;
};

struct PedigreePairCoefficient {
    CatId cat_a_id{};
    CatId cat_b_id{};
    double coefficient{};
};

struct RoomAttributes {
    double comfort{};
    double stimulation{};
    double health{};
    double mutation{};
    double appeal{};

    bool operator==(const RoomAttributes&) const = default;
};

struct RoomSnapshot {
    RoomId id;
    std::vector<CatId> residents;
    std::optional<RoomAttributes> attributes;
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
    bool read_sexuality{};
    bool read_breeding_eligibility{};
    bool read_relationships{};
    bool read_room_assignments{};
    bool read_room_attributes{};
    bool read_room_capacities{};
};

struct HouseSnapshot {
    std::uint64_t snapshot_id{};
    std::uint64_t scene_generation{};
    std::optional<std::int64_t> game_day;
    std::string source_save_name;
    std::vector<CatSnapshot> cats;
    std::vector<RoomSnapshot> rooms;
    std::vector<PedigreeEntry> pedigree;
    std::vector<PedigreePairCoefficient> pedigree_pair_coefficients;
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
