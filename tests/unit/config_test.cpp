#include "config/Config.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

namespace {

// Sets FITPLAN_* environment variables for the duration of one test and clears
// them again afterwards, so cases stay independent.
class ConfigEnvTest : public ::testing::Test {
protected:
    void set_env(const char* key, const char* value) {
        ::setenv(key, value, /*overwrite=*/1);
        keys_.push_back(key);
    }

    void TearDown() override {
        for (const char* key : keys_) {
            ::unsetenv(key);
        }
    }

private:
    std::vector<const char*> keys_;
};

TEST_F(ConfigEnvTest, UsesDefaultsWhenEnvironmentIsEmpty) {
    const auto cfg = fitplan::Config::from_env();

    EXPECT_EQ(cfg.host, "0.0.0.0");
    EXPECT_EQ(cfg.port, 8080);
    EXPECT_EQ(cfg.database_path, "fitplan.db");
    EXPECT_EQ(cfg.jwt_ttl_seconds, 24 * 60 * 60);
    EXPECT_EQ(cfg.thread_count, 0u);
    EXPECT_TRUE(cfg.uses_insecure_jwt_secret());
}

TEST_F(ConfigEnvTest, ReadsOverridesFromEnvironment) {
    set_env("FITPLAN_HOST", "127.0.0.1");
    set_env("FITPLAN_PORT", "9090");
    set_env("FITPLAN_DB_PATH", "/tmp/fitplan-test.db");
    set_env("FITPLAN_JWT_SECRET", "a-real-secret");
    set_env("FITPLAN_JWT_TTL_SECONDS", "3600");
    set_env("FITPLAN_THREADS", "4");

    const auto cfg = fitplan::Config::from_env();

    EXPECT_EQ(cfg.host, "127.0.0.1");
    EXPECT_EQ(cfg.port, 9090);
    EXPECT_EQ(cfg.database_path, "/tmp/fitplan-test.db");
    EXPECT_EQ(cfg.jwt_ttl_seconds, 3600);
    EXPECT_EQ(cfg.thread_count, 4u);
    EXPECT_FALSE(cfg.uses_insecure_jwt_secret());
}

TEST_F(ConfigEnvTest, InvalidIntegerFallsBackToDefault) {
    set_env("FITPLAN_PORT", "not-a-number");

    const auto cfg = fitplan::Config::from_env();

    EXPECT_EQ(cfg.port, 8080);
}

}  // namespace
