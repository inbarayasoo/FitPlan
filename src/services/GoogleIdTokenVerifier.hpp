#pragma once

#include <optional>
#include <string>

#include "services/GoogleJwks.hpp"

namespace fitplan::services {

// The identity facts we trust once a Google ID token has passed every check.
struct GoogleIdentity {
    std::string subject;  // Google's stable per-user id (the "sub" claim)
    std::string email;
    bool email_verified = false;
    std::string name;  // display name; may be empty
};

// Verifies "Sign in with Google" ID tokens for one OAuth client. Holds the
// client id the token's audience must match and a GoogleJwksCache it asks for
// Google's signing keys.
class GoogleIdTokenVerifier {
public:
    GoogleIdTokenVerifier(std::string client_id, GoogleJwksCache& keys);

    // Full check: RS256 signature against Google's key for the token's `kid`,
    // issuer is accounts.google.com, audience equals our client id, not expired
    // (60s leeway), `sub` present. Returns the identity on success, std::nullopt
    // on any failure. Never throws.
    [[nodiscard]] std::optional<GoogleIdentity> verify(const std::string& id_token) const;

private:
    std::string client_id_;
    GoogleJwksCache& keys_;
};

}  // namespace fitplan::services