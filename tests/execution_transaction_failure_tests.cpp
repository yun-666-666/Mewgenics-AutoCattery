#include "execution_plan_fixture.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunExecutionTransactionFailureTests() {
    using namespace execution_test;

    {
        SealedFixture fixture;
        FakeWrite write;
        auto read = fixture.Reader();
        FakeBackup backup;
        backup.succeed = false;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::BackupFailed);
        AC_CHECK(write.calls == 0);
        AC_CHECK(journal.writes == 0);
    }

    {
        SealedFixture fixture;
        FakeWrite write;
        auto read = fixture.Reader();
        FakeBackup backup;
        FakeRecovery recovery;
        recovery.succeed = false;
        FakeJournal journal;
        const auto result = Execute(
            fixture, write, read, backup, recovery, journal);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::RecoveryPackageFailed);
        AC_CHECK(write.calls == 0);
        AC_CHECK(journal.writes == 0);
    }

    for (const int fail_call : {1, 3, 5}) {
        SealedFixture fixture;
        FakeWrite write;
        write.fail_call = fail_call;
        auto read = fixture.Reader();
        FakeBackup backup;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(!result.committed);
        AC_CHECK(write.calls == fail_call);
        AC_CHECK(
            result.failure_reason ==
                execution::FailureReason::MoveFailed ||
            result.failure_reason ==
                execution::FailureReason::CullFailed);
    }

    for (const int fail_call : {2, 3, 4}) {
        SealedFixture fixture(1, 3);
        FakeWrite write;
        write.fail_call = fail_call;
        auto read = fixture.Reader();
        FakeBackup backup;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(!result.committed);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::CullFailed);
        AC_CHECK(write.calls == fail_call);
    }

    {
        SealedFixture fixture;
        FakeWrite write;
        auto read = fixture.Reader();
        read.fail_move_verify = 1;
        FakeBackup backup;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::MoveVerificationFailed);
        AC_CHECK(write.calls == 1);
        AC_CHECK(write.restores == 1);
    }

    {
        SealedFixture fixture;
        FakeWrite write;
        auto read = fixture.Reader();
        read.fail_observe = 5;
        FakeBackup backup;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::PreconditionsChanged);
        AC_CHECK(write.culls == 0);
    }

    {
        SealedFixture fixture;
        FakeWrite write;
        write.fail_call = 5;
        write.fail_restore = 1;
        auto read = fixture.Reader();
        FakeBackup backup;
        FakeRecovery recovery;
        FakeJournal journal;
        const auto result =
            Execute(fixture, write, read, backup, recovery, journal);
        AC_CHECK(
            result.failure_reason ==
            execution::FailureReason::RollbackFailed);
        AC_CHECK(
            journal.statuses.back() ==
            execution::JournalStatus::ManualRecoveryRequired);
    }
}

}  // namespace autocattery::tests
