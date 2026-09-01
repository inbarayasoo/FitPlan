#include "util/Jwt.hpp"

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

#include <chrono>
#include <string>

namespace {

using fitplan::util::verify_access_token;

constexpr const char* kSecret = "test-secret";

// One hour in the future. A token that expires at this instant is unquestionably
// still valid, so every rejection below is caused by the property the test is
// probing, never by expiry.
std::chrono::system_clock::time_point in_one_hour() {
    return std::chrono::system_clock::now() + std::chrono::hours{1};
}

// How the crafted token is signed.
enum class Signature { kHs256, kNone };

// Everything that can be "off" about a token, each field defaulting to a valid
// value. A test overrides just the one part it wants to attack.
struct TokenSpec {
    std::string issuer = "fitplan";
    bool include_role = true;
    std::string subject = "1";
    Signature signature = Signature::kHs256;
};

// Builds and signs a token to `spec`. Used to manufacture the adversarial tokens
// that make_access_token() would never produce.
std::string build_token(const TokenSpec& spec) {
    auto builder = jwt::create();
    builder.set_type("JWT");
    builder.set_subject(spec.subject);
    builder.set_issued_at(std::chrono::system_clock::now());
    builder.set_expires_at(in_one_hour());
    if (!spec.issuer.empty()) {
        builder.set_issuer(spec.issuer);
    }
    if (spec.include_role) {
        builder.set_payload_claim("role", jwt::claim(std::string{"coach"}));
    }
    if (spec.signature == Signature::kNone) {
        return builder.sign(jwt::algorithm::none{});
    }
    return builder.sign(jwt::algorithm::hs256{kSecret});
}

// The classic "alg: none" downgrade: the attacker drops the signature and sets
// the header algorithm to "none", hoping the server skips signature checking.
// verify_access_token() only allows HS256, so it must refuse this.
TEST(JwtHardeningTest, RejectsTheAlgNoneToken) {
    const std::string token = build_token({.signature = Signature::kNone});

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

// Correctly signed with our secret, unexpired - but minted by someone else, so
// the issuer is wrong. The issuer check is what stands between us and tokens
// from a different system that happens to share a secret.
TEST(JwtHardeningTest, RejectsAWrongIssuer) {
    const std::string token = build_token({.issuer = "evil-corp"});

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

// No "iss" claim at all. A token must positively assert it is ours, not merely
// fail to say it is not.
TEST(JwtHardeningTest, RejectsAMissingIssuer) {
    const std::string token = build_token({.issuer = ""});

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

// Signature and issuer are fine, but the token carries no "role" claim. The
// function must report this as invalid rather than throw while reading a claim
// that is not there.
TEST(JwtHardeningTest, RejectsAWellSignedTokenWithNoRoleClaim) {
    const std::string token = build_token({.include_role = false});

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

// The subject is meant to be a numeric user id. A non-numeric one must not slip
// through, and must not crash the std::stoll that parses it.
TEST(JwtHardeningTest, RejectsANonNumericSubject) {
    const std::string token = build_token({.subject = "not-a-number"});

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

// Inputs that are not a three-part JWT at all: too few segments, and three
// segments that are not valid base64url. Each must return nullopt, never throw.
TEST(JwtHardeningTest, RejectsStructurallyBrokenTokens) {
    EXPECT_FALSE(verify_access_token("only-one-segment", kSecret).has_value());
    EXPECT_FALSE(verify_access_token("two.segments", kSecret).has_value());
    EXPECT_FALSE(verify_access_token("alpha.beta.gamma", kSecret).has_value());
    EXPECT_FALSE(verify_access_token("!!!.$$$.???", kSecret).has_value());
}

}  // namespace
