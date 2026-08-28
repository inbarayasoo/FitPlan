#include "middleware/JwtAuthMiddleware.hpp"

#include <gtest/gtest.h>

#include <crow.h>

#include <string>

#include "util/Jwt.hpp"

namespace {

using fitplan::middleware::JwtAuthMiddleware;
using fitplan::util::make_access_token;

constexpr const char* kSecret = "mw-test-secret";

JwtAuthMiddleware::context run_before(const std::string& auth_header) {
    JwtAuthMiddleware mw;
    mw.secret = kSecret;

    crow::request req;
    if (!auth_header.empty()) {
        req.add_header("Authorization", auth_header);
    }
    crow::response res;
    JwtAuthMiddleware::context ctx;
    mw.before_handle(req, res, ctx);
    return ctx;
}

TEST(JwtAuthMiddlewareTest, PopulatesClaimsForAValidBearerToken) {
    const std::string token = make_access_token(7, "trainee", kSecret, 3600);

    const auto ctx = run_before("Bearer " + token);

    ASSERT_TRUE(ctx.claims.has_value());
    EXPECT_EQ(ctx.claims->user_id, 7);
    EXPECT_EQ(ctx.claims->role, "trainee");
}

TEST(JwtAuthMiddlewareTest, LeavesClaimsEmptyWhenNoHeader) {
    const auto ctx = run_before("");

    EXPECT_FALSE(ctx.claims.has_value());
}

TEST(JwtAuthMiddlewareTest, LeavesClaimsEmptyWhenSchemeIsNotBearer) {
    const std::string token = make_access_token(7, "trainee", kSecret, 3600);

    const auto ctx = run_before("Basic " + token);

    EXPECT_FALSE(ctx.claims.has_value());
}

TEST(JwtAuthMiddlewareTest, LeavesClaimsEmptyForATokenSignedWithAnotherSecret) {
    const std::string token = make_access_token(7, "trainee", "other-secret", 3600);

    const auto ctx = run_before("Bearer " + token);

    EXPECT_FALSE(ctx.claims.has_value());
}

}  // namespace