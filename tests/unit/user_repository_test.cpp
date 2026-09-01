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

}  // namespace
