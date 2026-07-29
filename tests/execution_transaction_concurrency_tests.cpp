#include "execution_plan_fixture.hpp"
#include "test_support.hpp"

#include <atomic>

namespace autocattery::tests {

void RunExecutionTransactionConcurrencyTests() {
    using namespace execution_test;

    SealedFixture fixture(1, 0);
    FakeWrite write;
    write.delay = std::chrono::milliseconds(50);
    auto read = fixture.Reader();
    FakeBackup backup;
    FakeRecovery recovery;
    FakeJournal journal;
    execution::TransactionExecutor executor(
        write, read, backup, recovery, journal);
    std::atomic<bool> first_committed{};
    std::thread first([&] {
        first_committed = executor.Execute({
            *fixture.approval.plan,
            *fixture.approval.authorization,
            "fixture.sav",
            true
        }).committed;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto concurrent = executor.Execute({
        *fixture.approval.plan,
        *fixture.approval.authorization,
        "fixture.sav",
        true
    });
    first.join();
    AC_CHECK(first_committed);
    AC_CHECK(
        concurrent.failure_reason ==
        execution::FailureReason::Busy);

    const auto duplicate = executor.Execute({
        *fixture.approval.plan,
        *fixture.approval.authorization,
        "fixture.sav",
        true
    });
    AC_CHECK(
        duplicate.failure_reason ==
        execution::FailureReason::AlreadyExecuted);

    SealedFixture unsupported_fixture;
    FakeWrite unsupported_write;
    unsupported_write.capability =
        execution::WriteCapability::Unsupported;
    auto unsupported_read = unsupported_fixture.Reader();
    FakeBackup unsupported_backup;
    FakeRecovery unsupported_recovery;
    FakeJournal unsupported_journal;
    const auto unsupported = Execute(
        unsupported_fixture,
        unsupported_write,
        unsupported_read,
        unsupported_backup,
        unsupported_recovery,
        unsupported_journal);
    AC_CHECK(
        unsupported.failure_reason ==
        execution::FailureReason::Unsupported);
    AC_CHECK(unsupported_backup.calls == 0);
    AC_CHECK(unsupported_write.calls == 0);

    execution::UnsupportedGameWriteAdapter real_adapter;
    AC_CHECK(
        real_adapter.Capability() ==
        execution::WriteCapability::Unsupported);
}

}  // namespace autocattery::tests
