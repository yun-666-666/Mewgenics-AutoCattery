#include "auto_cattery/snapshot/domain.hpp"

#include <algorithm>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot ValidSnapshot() {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 1;
    house.scene_generation = 7;
    house.cats = {
        {.id = 11, .display_name = "one", .room_id = "Floor1_Large"},
        {.id = 12, .display_name = "two"}
    };
    house.rooms = {
        {.id = "Floor1_Large", .residents = {11}}
    };
    house.capabilities.stable_cat_id = true;
    house.capabilities.read_room_assignments = true;
    return house;
}

bool HasIssue(
    const snapshot::SnapshotValidation& validation,
    std::string_view code) {
    return std::any_of(
        validation.issues.begin(),
        validation.issues.end(),
        [code](const snapshot::ValidationIssue& issue) {
            return issue.code == code;
        });
}

}  // namespace

void RunSnapshotDomainTests() {
    const auto valid = snapshot::Validate(ValidSnapshot());
    AC_CHECK(valid.Valid());
    AC_CHECK(valid.ErrorCount() == 0);
    AC_CHECK(valid.WarningCount() == 3);

    auto duplicate_cat = ValidSnapshot();
    duplicate_cat.cats.push_back(duplicate_cat.cats.front());
    const auto duplicate_cat_result = snapshot::Validate(duplicate_cat);
    AC_CHECK(!duplicate_cat_result.Valid());
    AC_CHECK(HasIssue(
        duplicate_cat_result,
        "duplicate-or-invalid-cat-id"));

    auto missing_cat = ValidSnapshot();
    missing_cat.rooms.front().residents.push_back(99);
    const auto missing_cat_result = snapshot::Validate(missing_cat);
    AC_CHECK(!missing_cat_result.Valid());
    AC_CHECK(HasIssue(
        missing_cat_result,
        "room-references-missing-cat"));

    auto multiple_rooms = ValidSnapshot();
    multiple_rooms.rooms.push_back({
        .id = "Attic",
        .residents = {11}
    });
    const auto multiple_rooms_result =
        snapshot::Validate(multiple_rooms);
    AC_CHECK(!multiple_rooms_result.Valid());
    AC_CHECK(HasIssue(
        multiple_rooms_result,
        "cat-assigned-to-multiple-rooms"));

    auto mismatch = ValidSnapshot();
    mismatch.cats.front().room_id = "Attic";
    const auto mismatch_result = snapshot::Validate(mismatch);
    AC_CHECK(!mismatch_result.Valid());
    AC_CHECK(HasIssue(
        mismatch_result,
        "cat-references-missing-room"));
}

}  // namespace autocattery::tests
