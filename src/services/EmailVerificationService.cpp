#include "services/EmailVerificationService.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>

#include "services/EmailVerificationError.hpp"
#include "util/VerificationCode.hpp"

namespace fitplan::services {

namespace {

std::string verification_email_body(const std::string& display_name, const std::string& code,
                                    const std::string& base_url, int ttl_seconds) {
    return "Hi " + display_name +
           ",\n\n"
           "Your FitPlan verification code is:\n\n"
           "    " +
           code +
           "\n\n"
           "Enter it at " +
           base_url +
           " to finish setting up your account.\n"
           "The code expires in " +
           std::to_string(ttl_seconds / 60) +
           " minutes. If you did not create a FitPlan account, ignore this email.\n\n"
           "- FitPlan";
}

}  // namespace

EmailVerificationService::EmailVerificationService(
    repositories::UserRepository& users, repositories::EmailVerificationTokenRepository& tokens,
    EmailSender send_email, util::Clock clock, std::string public_base_url,
    EmailVerificationPolicy policy)
    : users_(users),
      tokens_(tokens),
      send_email_(std::move(send_email)),
      clock_(std::move(clock)),
      public_base_url_(std::move(public_base_url)),
      policy_(policy) {}

void EmailVerificationService::start_for(const models::User& user) {
    const std::string code = util::generate_verification_code();
    const std::string issued_at = clock_();
    const std::string expires_at = util::iso_utc_shift(issued_at, policy_.code_ttl_seconds);

    tokens_.upsert(user.id, util::sha256_hex(code), expires_at, issued_at);

    const EmailMessage message{
        user.email,
        user.display_name,
        "Verify your FitPlan email",
        verification_email_body(user.display_name, code, public_base_url_,
                                policy_.code_ttl_seconds),
    };

    try {
        send_email_(message);
    } catch (const std::exception& ex) {
        spdlog::warn("email verification: could not send code to <{}>: {}", user.email, ex.what());
    }
}

models::User EmailVerificationService::verify(const std::string& email, const std::string& code) {
    const std::optional<models::User> user = users_.find_by_email(email);
    if (!user.has_value()) {
        throw EmailVerificationError(EmailVerificationErrorKind::kNotPending,
                                     "no verification is pending for this address");
    }

    const std::optional<models::EmailVerificationToken> token = tokens_.find_for_user(user->id);
    if (!token.has_value()) {
        throw EmailVerificationError(EmailVerificationErrorKind::kNotPending,
                                     "no verification is pending for this address");
    }

    if (token->attempts >= policy_.max_attempts) {
        tokens_.delete_for_user(user->id);
        throw EmailVerificationError(EmailVerificationErrorKind::kTooManyAttempts,
                                     "too many incorrect attempts; request a new code");
    }

    if (clock_() >= token->expires_at) {
        tokens_.delete_for_user(user->id);
        throw EmailVerificationError(EmailVerificationErrorKind::kCodeExpired,
                                     "the code has expired; request a new one");
    }

    if (util::sha256_hex(code) != token->code_hash) {
        tokens_.increment_attempts(user->id);
        throw EmailVerificationError(EmailVerificationErrorKind::kCodeMismatch, "incorrect code");
    }

    users_.mark_email_verified(user->id);
    tokens_.delete_for_user(user->id);
    return users_.find_by_id(user->id).value();
}

void EmailVerificationService::resend(const std::string& email) {
    const std::optional<models::User> user = users_.find_by_email(email);
    if (!user.has_value() || user->auth_provider != "local" || user->email_verified) {
        return;
    }

    if (const auto token = tokens_.find_for_user(user->id)) {
        const std::string earliest_resend =
            util::iso_utc_shift(token->issued_at, policy_.resend_min_gap_seconds);
        if (clock_() < earliest_resend) {
            return;
        }
    }

    start_for(*user);
}

}  // namespace fitplan::services