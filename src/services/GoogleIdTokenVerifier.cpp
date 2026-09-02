#include "services/GoogleIdTokenVerifier.hpp"

#include <jwt-cpp/jwt.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace fitplan::services {

namespace {

// Google mints tokens with one of these two issuer strings.
bool issuer_is_google(const std::string& iss) {
    return iss == "https://accounts.google.com" || iss == "accounts.google.com";
}

// Google sends email_verified as a JSON boolean; some older tokens sent the
// string "true". Accept either, treat anything else as false.
bool claim_is_true(const jwt::claim& value) {
    if (value.get_type() == jwt::json::type::boolean) {
        return value.as_boolean();
    }
    if (value.get_type() == jwt::json::type::string) {
        return value.as_string() == "true";
    }
    return false;
}

}  // namespace

GoogleIdTokenVerifier::GoogleIdTokenVerifier(std::string client_id, GoogleJwksCache& keys)
    : client_id_(std::move(client_id)), keys_(keys) {}

std::optional<GoogleIdentity> GoogleIdTokenVerifier::verify(const std::string& id_token) const {
    try {
        const auto token = jwt::decode(id_token);

        // Which Google key signed this? The header names it by "kid".
        if (!token.has_key_id()) {
            return std::nullopt;
        }
        const std::optional<std::string> pem = keys_.public_key_pem(token.get_key_id());
        if (!pem.has_value()) {
            return std::nullopt;
        }

        // Signature (RS256 against that key), audience, and expiry (60s leeway).
        jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(*pem))
            .with_audience(client_id_)
            .leeway(60)
            .verify(token);

        // Issuer and subject: checked by hand so both Google issuer spellings pass.
        if (!token.has_issuer() || !issuer_is_google(token.get_issuer())) {
            return std::nullopt;
        }
        if (!token.has_subject() || token.get_subject().empty()) {
            return std::nullopt;
        }

        GoogleIdentity identity;
        identity.subject = token.get_subject();
        if (token.has_payload_claim("email")) {
            identity.email = token.get_payload_claim("email").as_string();
        }
        if (token.has_payload_claim("email_verified")) {
            identity.email_verified = claim_is_true(token.get_payload_claim("email_verified"));
        }
        if (token.has_payload_claim("name")) {
            identity.name = token.get_payload_claim("name").as_string();
        }
        return identity;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace fitplan::services