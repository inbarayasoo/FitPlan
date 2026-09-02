#pragma once

// Test-only helper: mint a Google-style ID token, signed with a local RSA test
// key, with every field overridable so a test can break exactly one thing.

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <string>

#include "RsaTestKey.hpp"

namespace fitplan::testutil {

inline constexpr const char* kTestGoogleClientId = "fitplan-test.apps.googleusercontent.com";

struct GoogleTokenOptions {
    std::string key_id;  // header "kid"; empty -> use the signing key's own kid
    std::string issuer = "https://accounts.google.com";
    std::string audience = kTestGoogleClientId;
    std::string subject = "google-sub-42";
    std::string email = "gina@example.com";
    bool include_email_verified = true;
    bool email_verified = true;
    std::string name = "Gina G";
    std::chrono::system_clock::duration lifetime = std::chrono::hours{1};
};

inline std::string mint_google_id_token(const RsaTestKey& signing_key,
                                        const GoogleTokenOptions& opt = {}) {
    const auto now = std::chrono::system_clock::now();
    auto builder = jwt::create()
                       .set_key_id(opt.key_id.empty() ? signing_key.kid : opt.key_id)
                       .set_issuer(opt.issuer)
                       .set_audience(opt.audience)
                       .set_issued_at(now)
                       .set_expires_at(now + opt.lifetime)
                       .set_subject(opt.subject)
                       .set_payload_claim("email", jwt::claim(opt.email))
                       .set_payload_claim("name", jwt::claim(opt.name));
    if (opt.include_email_verified) {
        builder.set_payload_claim("email_verified",
                                  jwt::claim(picojson::value(opt.email_verified)));
    }
    return builder.sign(jwt::algorithm::rs256("", signing_key.private_pem));
}

}  // namespace fitplan::testutil
