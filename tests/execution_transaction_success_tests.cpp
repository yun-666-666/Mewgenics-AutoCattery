#include "execution_plan_fixture.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunExecutionTransactionSuccessTests() {
    using namespace execution_test;

    SealedFixture fixture;
    FakeWrite write;
    auto read = fixture.Reader();
    FakeBackup backup;
    FakeRecovery recovery;
    FakeJournal journal;
    const auto result =
        Execute(fixture, write, read, backup, recovery, journal);

    AC_CHECK(result.committed);
    AC_CHECK(result.completed_moves == 3);
    AC_CHECK(result.completed_culls == 2);
    AC_CHECK(write.calls == 5);
    AC_CHECK(
        journal.statuses.front() ==
        execution::JournalStatus::Prepared);
    AC_CHECK(
        journal.statuses.back() ==
        execution::JournalStatus::Committed);

    SealedFixture move_only_fixture(3, 0);
    FakeWrite move_only_write;
    move_only_write.capability =
        execution::WriteCapability::MoveOnly;
    auto move_only_read = move_only_fixture.Reader();
    FakeBackup move_only_backup;
    FakeRecovery move_only_recovery;
    FakeJournal move_only_journal;
    const auto move_only = Execute(
        move_only_fixture,
        move_only_write,
        move_only_read,
        move_only_backup,
        move_only_recovery,
        move_only_journal);
    AC_CHECK(move_only.committed);
    AC_CHECK(move_only_write.culls == 0);
}

}  // namespace autocattery::tests
