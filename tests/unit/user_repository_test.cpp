#include "repositories/UserRepository.hpp"

#include <gtest/gtest.h>

#include <string>

#include "db/Database.hpp"
#include "models/User.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::User;
using fitplan::repositories::UserRepository;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class UserRepositoryTest : public ::testing::Test {
protected:
    User sample() const {
        User u;
        u.email = "coach@example.com";
        u.password_hash = "not-a-real-hash";
        u.role = "coach";
        u.display_name = "Coach One";
        return u;
    }

    // An account as a Google login would create it: no password, a provider of
    // "google", and the "sub" claim from the verified id token.
    User google_sample() const {
        User u;
        u.email = "gina@example.com";
        u.role = "trainee";
        u.display_name = "Gina Google";
        u.auth_provider = "google";
        u.google_sub = "google-sub-123";
        return u;
    }

    Database db_{":memory:", migrations_dir()};
    UserRepository repo_{db_.connection()};
};

TEST_F(UserRepositoryTest, CreateAssignsIdAndCreatedAt) {
    const User saved = repo_.create(sample());

    EXPECT_GT(saved.id, 0);
    EXPECT_FALSE(saved.created_at.empty());
    EXPECT_EQ(saved.email, "coach@example.com");
    EXPECT_EQ(saved.role, "coach");
}

TEST_F(UserRepositoryTest, FindByEmailRoundTrips) {
    const User saved = repo_.create(sample());

    const auto found = repo_.find_by_email("coach@example.com");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);
    EXPECT_EQ(found->password_hash, "not-a-real-hash");
}

TEST_F(UserRepositoryTest, FindByEmailReturnsNulloptWhenAbsent) {
    EXPECT_FALSE(repo_.find_by_email("nobody@example.com").has_value());
}

TEST_F(UserRepositoryTest, EmailExistsReflectsInserts) {
    EXPECT_FALSE(repo_.email_exists("coach@example.com"));
    repo_.create(sample());
    EXPECT_TRUE(repo_.email_exists("coach@example.com"));
}

TEST_F(UserRepositoryTest, DuplicateEmailThrows) {
    repo_.create(sample());
    EXPECT_THROW(repo_.create(sample()), SQLite::Exception);
}

TEST_F(UserRepositoryTest, CreateDefaultsAuthProviderToLocalAndLeavesGoogleSubEmpty) {
    const User saved = repo_.create(sample());

    EXPECT_EQ(saved.auth_provider, "local");
    EXPECT_TRUE(saved.google_sub.empty());
}

TEST_F(UserRepositoryTest, TwoLocalAccountsDoNotCollideOnAnEmptyGoogleSub) {
    repo_.create(sample());

    User other = sample();
    other.email = "coach2@example.com";
    // Empty google_sub must be stored as SQL NULL, and NULLs do not collide
    // under the UNIQUE index - two empty strings would.
    EXPECT_NO_THROW(repo_.create(other));
}

TEST_F(UserRepositoryTest, CreateStoresAGoogleAccountWithNoPassword) {
    const User saved = repo_.create(google_sample());

    EXPECT_GT(saved.id, 0);
    EXPECT_EQ(saved.auth_provider, "google");
    EXPECT_EQ(saved.google_sub, "google-sub-123");
    EXPECT_TRUE(saved.password_hash.empty());
}

TEST_F(UserRepositoryTest, FindByGoogleSubRoundTrips) {
    const User saved = repo_.create(google_sample());

    const auto found = repo_.find_by_google_sub("google-sub-123");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);
    EXPECT_EQ(found->email, "gina@example.com");
}

TEST_F(UserRepositoryTest, FindByGoogleSubReturnsNulloptWhenAbsent) {
    repo_.create(sample());
    EXPECT_FALSE(repo_.find_by_google_sub("no-such-sub").has_value());
}

TEST_F(UserRepositoryTest, LinkGoogleAttachesTheIdentityWithoutChangingTheProvider) {
    const User saved = repo_.create(sample());

    repo_.link_google(saved.id, "sub-xyz");

    const auto by_id = repo_.find_by_id(saved.id);
    ASSERT_TRUE(by_id.has_value());
    EXPECT_EQ(by_id->google_sub, "sub-xyz");
    EXPECT_EQ(by_id->auth_provider, "local");  // records how the account was created

    const auto by_sub = repo_.find_by_google_sub("sub-xyz");
    ASSERT_TRUE(by_sub.has_value());
    EXPECT_EQ(by_sub->id, saved.id);
}

}  // namespace
