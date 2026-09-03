#pragma once

#include <string>

#include "models/User.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/EmailSender.hpp"
#include "util/Clock.hpp"

namespace fitplan::services {

// Tunable policy for the email-verification flow. Grouped so the knobs live in
// one place and tests can override them.
struct EmailVerificationPolicy {
    int code_ttl_seconds = 600;       // a code is good for 10 minutes
    int max_attempts = 5;             // wrong guesses before the code is burned
    int resend_min_gap_seconds = 60;  // shortest spacing between "resend" mails
};

// Issues, re-issues and checks the six-digit code that confirms a local
// account's email address. Crow-free: it borrows the two repositories, and both
// the email transport and "now" are injected so the whole flow is testable
// without a network or a real clock.
class EmailVerificationService {
public:
    EmailVerificationService(repositories::UserRepository& users,
                             repositories::EmailVerificationTokenRepository& tokens,
                             EmailSender send_email, util::Clock clock, std::string public_base_url,
                             EmailVerificationPolicy policy = {});

    // Generates a fresh code for `user`, stores only its hash, and emails it.
    // A failure of the email transport is logged, not thrown: a transient mail
    // outage must not undo a completed registration - the user can "resend".
    void start_for(const models::User& user);

    // Checks `code` for the account with this `email`. On success the account is
    // marked verified, the code is deleted, and the refreshed user is returned.
    // Throws EmailVerificationError on every failure:
    //   kNotPending      - unknown address, or no code outstanding
    //   kTooManyAttempts - the guess limit was already reached (code burned)
    //   kCodeExpired     - the code is past its lifetime (code burned)
    //   kCodeMismatch    - wrong code; this guess has been counted
    models::User verify(const std::string& email, const std::string& code);

    // Sends a new code for this address if one is due. Silent for an unknown
    // address, a non-local account, an already-verified account, or a request
    // that arrives within resend_min_gap_seconds of the last one - the caller
    // must not leak which case applied.
    void resend(const std::string& email);

private:
    repositories::UserRepository& users_;
    repositories::EmailVerificationTokenRepository& tokens_;
    EmailSender send_email_;
    util::Clock clock_;
    std::string public_base_url_;
    EmailVerificationPolicy policy_;
};

}  // namespace fitplan::services