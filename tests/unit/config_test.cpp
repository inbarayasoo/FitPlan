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
    EXPECT_EQ(cfg.migrations_dir, "src/db/migrations");
    EXPECT_EQ(cfg.jwt_ttl_seconds, 24 * 60 * 60);
    EXPECT_EQ(cfg.thread_count, 0u);
    EXPECT_EQ(cfg.web_dir, "web");
    EXPECT_EQ(cfg.docs_dir, "docs");
    EXPECT_EQ(cfg.google_client_id, "");
    EXPECT_EQ(cfg.brevo_api_key, "");
    EXPECT_EQ(cfg.email_from, "no-reply@fitplan.dev");
    EXPECT_EQ(cfg.email_from_name, "FitPlan");
    EXPECT_EQ(cfg.public_base_url, "http://localhost:8080");
    EXPECT_TRUE(cfg.uses_insecure_jwt_secret());
    EXPECT_FALSE(cfg.google_sign_in_enabled());
    EXPECT_FALSE(cfg.transactional_email_enabled());
}

TEST_F(ConfigEnvTest, ReadsOverridesFromEnvironment) {
    set_env("FITPLAN_HOST", "127.0.0.1");
    set_env("FITPLAN_PORT", "9090");
    set_env("FITPLAN_DB_PATH", "/tmp/fitplan-test.db");
    set_env("FITPLAN_MIGRATIONS_DIR", "/opt/fitplan/migrations");
    set_env("FITPLAN_JWT_SECRET", "a-real-secret");
    set_env("FITPLAN_JWT_TTL_SECONDS", "3600");
    set_env("FITPLAN_THREADS", "4");
    set_env("FITPLAN_WEB_DIR", "/opt/fitplan/web");
    set_env("FITPLAN_DOCS_DIR", "/opt/fitplan/docs");
    set_env("FITPLAN_GOOGLE_CLIENT_ID", "123-abc.apps.googleusercontent.com");
    set_env("FITPLAN_BREVO_API_KEY", "xkeysib-secret");
    set_env("FITPLAN_EMAIL_FROM", "hello@myfit.app");
    set_env("FITPLAN_EMAIL_FROM_NAME", "MyFit");
    set_env("FITPLAN_PUBLIC_BASE_URL", "https://myfit.app");

    const auto cfg = fitplan::Config::from_env();

    EXPECT_EQ(cfg.host, "127.0.0.1");
    EXPECT_EQ(cfg.port, 9090);
    EXPECT_EQ(cfg.database_path, "/tmp/fitplan-test.db");
    EXPECT_EQ(cfg.migrations_dir, "/opt/fitplan/migrations");
    EXPECT_EQ(cfg.jwt_ttl_seconds, 3600);
    EXPECT_EQ(cfg.thread_count, 4u);
    EXPECT_EQ(cfg.web_dir, "/opt/fitplan/web");
    EXPECT_EQ(cfg.docs_dir, "/opt/fitplan/docs");
    EXPECT_EQ(cfg.google_client_id, "123-abc.apps.googleusercontent.com");
    EXPECT_EQ(cfg.brevo_api_key, "xkeysib-secret");
    EXPECT_EQ(cfg.email_from, "hello@myfit.app");
    EXPECT_EQ(cfg.email_from_name, "MyFit");
    EXPECT_EQ(cfg.public_base_url, "https://myfit.app");
    EXPECT_FALSE(cfg.uses_insecure_jwt_secret());
    EXPECT_TRUE(cfg.google_sign_in_enabled());
    EXPECT_TRUE(cfg.transactional_email_enabled());
}

TEST_F(ConfigEnvTest, InvalidIntegerFallsBackToDefault) {
    set_env("FITPLAN_PORT", "not-a-number");

    const auto cfg = fitplan::Config::from_env();

    EXPECT_EQ(cfg.port, 8080);
}

}  // namespace
