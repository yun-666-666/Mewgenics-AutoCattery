#include "auto_cattery/protection/sidecar.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::string SidecarWithRecords(std::size_t count) {
    std::ostringstream json;
    json << R"({"schema_version":1,"records":[)";
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            json << ',';
        }
        json << R"({"cat_id":)" << index + 1
             << R"(,"level":"NoCull","identity_token":"id-)"
             << index + 1 << R"("})";
    }
    json << R"(],"blacklist":[]})";
    return json.str();
}

void CheckClosed(
    std::string_view contents,
    protection::SidecarLoadStatus status) {
    const auto result = protection::ParseProtectionSidecar(contents);
    if (result.status != status) {
        std::cerr << "sidecar status mismatch: expected "
                  << static_cast<int>(status) << " got "
                  << static_cast<int>(result.status) << " for "
                  << contents << '\n';
    }
    AC_CHECK(result.status == status);
    AC_CHECK(result.destructive_actions_blocked);
    AC_CHECK(result.records.empty());
}

}  // namespace

void RunProtectionSidecarTests() {
    CheckClosed("", protection::SidecarLoadStatus::Empty);
    CheckClosed(" \r\n\t", protection::SidecarLoadStatus::Empty);
    CheckClosed("{", protection::SidecarLoadStatus::InvalidJson);
    CheckClosed(
        R"({"schema_version":0,"records":[],"blacklist":[]})",
        protection::SidecarLoadStatus::OldSchema);
    CheckClosed(
        R"({"schema_version":2,"records":[],"blacklist":[]})",
        protection::SidecarLoadStatus::FutureSchema);
    CheckClosed(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":1,"level":"NoCull","identity_token":"a"},)"
        R"({"cat_id":1,"level":"NoMove","identity_token":"a"})"
        R"(],"blacklist":[]})",
        protection::SidecarLoadStatus::InvalidSchema);

    const auto reused_ids = protection::ParseProtectionSidecar(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":1,"level":"NoMove","identity_token":"cat-a"},)"
        R"({"cat_id":1,"level":"NoCull","identity_token":"cat-b"})"
        R"(],"blacklist":[]})");
    AC_CHECK(reused_ids.status == protection::SidecarLoadStatus::Loaded);
    AC_CHECK(reused_ids.records.size() == 2);
    CheckClosed(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":18446744073709551615,"level":"NoCull",)"
        R"("identity_token":"a"}],"blacklist":[]})",
        protection::SidecarLoadStatus::InvalidSchema);
    CheckClosed(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":1,"level":"Invalid","identity_token":"a"})"
        R"(],"blacklist":[]})",
        protection::SidecarLoadStatus::InvalidSchema);
    CheckClosed(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":1,"level":"NoCull","identity_token":"a",)"
        R"("expires_on_day":1.5}],"blacklist":[]})",
        protection::SidecarLoadStatus::InvalidSchema);

    const auto valid = protection::ParseProtectionSidecar(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":7,"level":"FullyUnmanaged",)"
        R"("identity_token":"fingerprint","reason":"manual"})"
        R"(],"blacklist":[7]})");
    AC_CHECK(valid.status == protection::SidecarLoadStatus::Loaded);
    AC_CHECK(!valid.destructive_actions_blocked);
    AC_CHECK(valid.records.at(0).protection.level ==
             protection::ProtectionLevel::FullyUnmanaged);
    AC_CHECK(valid.blacklist.contains(7));

    const auto fixed = protection::ParseProtectionSidecar(
        R"({"schema_version":1,"records":[)"
        R"({"cat_id":8,"level":"NoCull","fixed_room":"Attic",)"
        R"("source_save_name":"fixture-campaign.sav"})"
        R"(],"blacklist":[]})");
    AC_CHECK(fixed.status == protection::SidecarLoadStatus::Loaded);
    AC_CHECK(fixed.records.at(0).identity_token.empty());
    AC_CHECK(fixed.records.at(0).fixed_room == "Attic");
    AC_CHECK(
        fixed.records.at(0).source_save_name == "fixture-campaign.sav");

    const auto missing = protection::LoadProtectionSidecar(
        std::filesystem::temp_directory_path() /
        "autocattery-sidecar-does-not-exist-stage08.json");
    AC_CHECK(missing.status == protection::SidecarLoadStatus::Missing);
    AC_CHECK(missing.destructive_actions_blocked);

    const auto thousand_json = SidecarWithRecords(1000);
    const auto started = std::chrono::steady_clock::now();
    const auto thousand =
        protection::ParseProtectionSidecar(thousand_json);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    AC_CHECK(thousand.status == protection::SidecarLoadStatus::Loaded);
    AC_CHECK(thousand.records.size() == 1000);
    AC_CHECK(elapsed < std::chrono::seconds(2));
    const auto repeated =
        protection::ParseProtectionSidecar(thousand_json);
    AC_CHECK(repeated.records.size() == thousand.records.size());
    AC_CHECK(repeated.records.at(999).identity_token ==
             thousand.records.at(999).identity_token);
}

}  // namespace autocattery::tests
