#pragma once

#include <cstdint>
#include <string>

#include "models/User.hpp"
#include "repositories/UserRepository.hpp"
#include "services/GoogleIdTokenVerifier.hpp"

namespace fitplan::services {

// The result of a successful register or login: the stored user plus a freshly
// signed access token for it.
struct AuthOutcome {
    models::User user;
    std::string access_token;
};

// The result of a Google login attempt. When `needs_role` is true, the token
// would create a brand-new account and no role was supplied: nothing was
// created, and the caller must ask the user for a role and call again with it.
// Otherwise `outcome` holds the token and user.
struct GoogleLoginResult {
    bool needs_role = false;
    AuthOutcome outcome;
};

// Registration, login, and "who does this token belong to" - the rules, with no
// HTTP or JSON in sight. Borrows the UserRepository; owns its JWT settings.
// `google_verifier` is optional: null when FITPLAN_GOOGLE_CLIENT_ID is unset, in
// which case login_with_google refuses.
class AuthService {
public:
    AuthService(repositories::UserRepository& users, std::string jwt_secret,
                std::int64_t jwt_ttl_seconds,
                const GoogleIdTokenVerifier* google_verifier = nullptr);

    // Hashes the password and stores a new user. Throws AuthError:
    //   kInvalidInput     - empty field, role not trainee/coach, password < 8
    //   kEmailAlreadyUsed - email already registered
    AuthOutcome register_user(const std::string& email, const std::string& password,
                              const std::string& role, const std::string& display_name);

    // Verifies the password against the stored hash. Throws AuthError:
    //   kInvalidCredentials - no such email, or wrong password
    AuthOutcome login(const std::string& email, const std::string& password);

    // Verifies a Google ID token and logs the user in, creating or linking a
    // local account as needed:
    //   1. an account already linked to this Google id  -> log in
    //   2. a password account with the same Google-verified email -> link, log in
    //   3. otherwise, a brand-new user:
    //        - `role` empty     -> return {needs_role = true}, create nothing
    //        - `role` valid     -> create the account with it (auth_provider =
    //                              'google', no password) and log in
    // `role` is ignored in cases 1 and 2. Issues the same FitPlan access token as
    // password login. Throws AuthError:
    //   kInvalidInput       - Google sign-in is not configured, the token carries
    //                         no email, or `role` is set but not trainee/coach
    //   kInvalidCredentials - the token failed verification
    //   kEmailAlreadyUsed   - a password account owns the email but the token's
    //                         email is not Google-verified, so linking is unsafe
    GoogleLoginResult login_with_google(const std::string& id_token, const std::string& role = "");

    // Loads the user named by a verified token's subject. Throws AuthError:
    //   kInvalidCredentials - the user no longer exists
    models::User authenticated_user(std::int64_t user_id);

private:
    [[nodiscard]] std::string sign_token_for(const models::User& user) const;

    repositories::UserRepository& users_;
    std::string jwt_secret_;
    std::int64_t jwt_ttl_seconds_;
    const GoogleIdTokenVerifier* google_verifier_;
};

}  // namespace fitplan::services