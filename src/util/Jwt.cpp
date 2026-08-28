#include "util/Jwt.hpp"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <exception>
#include <string>

namespace fitplan::util {

namespace {
constexpr const char* kIssuer = "fitplan";
}  // namespace

std::string make_access_token(std::int64_t user_id, const std::string& role,
                              const std::string& secret, std::int64_t ttl_seconds) {
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_issuer(kIssuer)
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::seconds{ttl_seconds})
        .set_subject(std::to_string(user_id))
        .set_payload_claim("role", jwt::claim(role))
        .sign(jwt::algorithm::hs256{secret});
}

std::optional<TokenClaims> verify_access_token(const std::string& token,
                                               const std::string& secret) {
    try {
        const auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(kIssuer)
            .verify(decoded);

        TokenClaims claims;
        claims.user_id = std::stoll(decoded.get_subject());
        claims.role = decoded.get_payload_claim("role").as_string();
        return claims;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace fitplan::util