#include "repositories/EmailVerificationTokenRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"

namespace {

using fitplan::db::Database;
using fitplan::repositories::EmailVerificationTokenRepository;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class EmailVerificationTokenRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_.connection().exec(
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES ('a@e.com','x','trainee','A'),"
            "       ('b@e.com','x','trainee','B')");
    }

    Database db_{":memory:", migrations_dir()};
    EmailVerificationTokenRepository repo_{db_.connection()};

    static constexpr std::int64_t kUserA = 1;
    static constexpr std::int64_t kUserB = 2;
    static constexpr const char* kExpiry = "2026-09-02 14:30:00";
    static constexpr const char* kIssued = "2026-09-02 14:20:00";
};

TEST_F(EmailVerificationTokenRepositoryTest, UpsertStoresTheRowThenReplacesItInPlace) {
    const auto first = repo_.upsert(kUserA, "hash-one", kExpiry, kIssued);
    EXPECT_GT(first.id, 0);
    EXPECT_EQ(first.user_id, kUserA);
    EXPECT_EQ(first.code_hash, "hash-one");
    EXPECT_EQ(first.expires_at, kExpiry);
    EXPECT_EQ(first.attempts, 0);
    EXPECT_EQ(first.issued_at, kIssued);

    repo_.increment_attempts(kUserA);
    repo_.increment_attempts(kUserA);

    const auto second =
        repo_.upsert(kUserA, "hash-two", "2026-09-02 15:00:00", "2026-09-02 14:50:00");
    EXPECT_EQ(second.id, first.id);  // same row, updated - not a new one
    EXPECT_EQ(second.code_hash, "hash-two");
    EXPECT_EQ(second.expires_at, "2026-09-02 15:00:00");
    EXPECT_EQ(second.issued_at, "2026-09-02 14:50:00");
    EXPECT_EQ(second.attempts, 0);  // a fresh code starts the counter over
}

TEST_F(EmailVerificationTokenRepositoryTest, FindForUserReturnsNulloptWhenThereIsNoPendingCode) {
    EXPECT_FALSE(repo_.find_for_user(kUserA).has_value());
}

TEST_F(EmailVerificationTokenRepositoryTest, IncrementAttemptsCountsUpAndIgnoresAMissingRow) {
    repo_.upsert(kUserA, "h", kExpiry, kIssued);

    repo_.increment_attempts(kUserA);
    repo_.increment_attempts(kUserA);
    repo_.increment_attempts(kUserA);
    EXPECT_EQ(repo_.find_for_user(kUserA)->attempts, 3);

    EXPECT_NO_THROW(repo_.increment_attempts(kUserB));  // no pending code for B
    EXPECT_FALSE(repo_.find_for_user(kUserB).has_value());
}

TEST_F(EmailVerificationTokenRepositoryTest, DeleteForUserReportsWhetherARowWasHitAndIsScoped) {
    repo_.upsert(kUserA, "ha", kExpiry, kIssued);
    repo_.upsert(kUserB, "hb", kExpiry, kIssued);

    EXPECT_TRUE(repo_.delete_for_user(kUserA));
    EXPECT_FALSE(repo_.delete_for_user(kUserA));  // already gone
    EXPECT_FALSE(repo_.find_for_user(kUserA).has_value());
    EXPECT_TRUE(repo_.find_for_user(kUserB).has_value());  // B untouched
}

TEST_F(EmailVerificationTokenRepositoryTest, DeletingTheUserCascadesToItsPendingCode) {
    repo_.upsert(kUserB, "hb", kExpiry, kIssued);
    ASSERT_TRUE(repo_.find_for_user(kUserB).has_value());

    db_.connection().exec("DELETE FROM users WHERE id = 2");

    EXPECT_FALSE(repo_.find_for_user(kUserB).has_value());
}

}  // namespace
