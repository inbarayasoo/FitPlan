#include "services/EmailVerificationService.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "models/User.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/EmailSender.hpp"
#include "services/EmailVerificationError.hpp"
#include "util/Clock.hpp"
#include "util/VerificationCode.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::User;
using fitplan::repositories::EmailVerificationTokenRepository;
using fitplan::repositories::UserRepository;
using fitplan::services::EmailMessage;
using fitplan::services::EmailVerificationError;
using fitplan::services::EmailVerificationErrorKind;
using fitplan::services::EmailVerificationService;
using fitplan::util::iso_utc_shift;
using fitplan::util::sha256_hex;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class EmailVerificationServiceTest : public ::testing::Test {
protected:
    // A clock the test moves by hand, and a transport that records instead of
    // sending. Both close over `this`, so the service sees every change.
    std::string now_ = "2026-09-02 12:00:00";
    fitplan::util::Clock clock_ = [this] { return now_; };

    std::vector<EmailMessage> sent_;
    bool sender_throws_ = false;
    fitplan::services::EmailSender sender_ = [this](const EmailMessage& m) {
        if (sender_throws_) {
            throw std::runtime_error("smtp down");
        }
        sent_.push_back(m);
    };

    Database db_{":memory:", migrations_dir()};
    UserRepository users_{db_.connection()};
    EmailVerificationTokenRepository tokens_{db_.connection()};
    EmailVerificationService svc_{users_, tokens_, sender_, clock_, "https://fitplan.test"};

    void advance_seconds(std::int64_t s) { now_ = iso_utc_shift(now_, s); }

    User make_local_user(const std::string& email = "u@e.com") {
        User u;
        u.email = email;
        u.password_hash = "x";
        u.role = "trainee";
        u.display_name = "Ursula";
        return users_.create(u);
    }

    std::string last_code() const {
        std::smatch m;
        const std::string& body = sent_.back().body;
        return std::regex_search(body, m, std::regex(R"(\d{6})")) ? m.str(0) : std::string{};
    }

    static EmailVerificationErrorKind kind_thrown_by(const std::function<void()>& action) {
        try {
            action();
        } catch (const EmailVerificationError& err) {
            return err.kind();
        }
        ADD_FAILURE() << "expected an EmailVerificationError";
        return EmailVerificationErrorKind::kNotPending;
    }
};

TEST_F(EmailVerificationServiceTest, StartForStoresOnlyAHashAndEmailsACode) {
    const User u = make_local_user();

    svc_.start_for(u);

    ASSERT_EQ(sent_.size(), 1u);
    EXPECT_EQ(sent_.front().to_email, "u@e.com");

    const std::string code = last_code();
    EXPECT_EQ(code.size(), 6u);

    const auto token = tokens_.find_for_user(u.id);
    ASSERT_TRUE(token.has_value());
    EXPECT_NE(token->code_hash, code);              // the plaintext is never stored
    EXPECT_EQ(token->code_hash, sha256_hex(code));  // only its hash
    EXPECT_EQ(token->issued_at, "2026-09-02 12:00:00");
    EXPECT_EQ(token->expires_at, "2026-09-02 12:10:00");  // issued + 600s
    EXPECT_EQ(token->attempts, 0);
}

TEST_F(EmailVerificationServiceTest, VerifyMarksTheAccountAndConsumesTheCode) {
    const User u = make_local_user();
    svc_.start_for(u);
    const std::string code = last_code();

    const User verified = svc_.verify("u@e.com", code);

    EXPECT_TRUE(verified.email_verified);
    EXPECT_TRUE(users_.find_by_id(u.id)->email_verified);
    EXPECT_FALSE(tokens_.find_for_user(u.id).has_value());  // code consumed

    // A second attempt has nothing left to check.
    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", code); }),
              EmailVerificationErrorKind::kNotPending);
}

TEST_F(EmailVerificationServiceTest, VerifyCountsAWrongGuessButTheRightCodeStillWorks) {
    const User u = make_local_user();
    svc_.start_for(u);
    const std::string code = last_code();
    const std::string wrong = code == "000000" ? "111111" : "000000";

    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", wrong); }),
              EmailVerificationErrorKind::kCodeMismatch);
    EXPECT_EQ(tokens_.find_for_user(u.id)->attempts, 1);

    EXPECT_NO_THROW(svc_.verify("u@e.com", code));  // still within the limit
}

TEST_F(EmailVerificationServiceTest, VerifyLocksAfterFiveWrongGuesses) {
    const User u = make_local_user();
    svc_.start_for(u);
    const std::string code = last_code();
    const std::string wrong = code == "000000" ? "111111" : "000000";

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", wrong); }),
                  EmailVerificationErrorKind::kCodeMismatch);
    }

    // Even the correct code is refused now, and the token is burned.
    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", code); }),
              EmailVerificationErrorKind::kTooManyAttempts);
    EXPECT_FALSE(tokens_.find_for_user(u.id).has_value());
    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", code); }),
              EmailVerificationErrorKind::kNotPending);
}

TEST_F(EmailVerificationServiceTest, VerifyRejectsAnExpiredCodeAndBurnsIt) {
    const User u = make_local_user();
    svc_.start_for(u);
    const std::string code = last_code();

    advance_seconds(600);  // now == expires_at exactly, which counts as expired

    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", code); }),
              EmailVerificationErrorKind::kCodeExpired);
    EXPECT_FALSE(tokens_.find_for_user(u.id).has_value());
}

TEST_F(EmailVerificationServiceTest, VerifyLooksTheSameForAnUnknownEmailAndForNoPendingCode) {
    make_local_user();  // exists, but start_for was never called

    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("ghost@e.com", "123456"); }),
              EmailVerificationErrorKind::kNotPending);
    EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", "123456"); }),
              EmailVerificationErrorKind::kNotPending);
}

TEST_F(EmailVerificationServiceTest, StartForSurvivesAFailingEmailTransport) {
    sender_throws_ = true;
    const User u = make_local_user();

    EXPECT_NO_THROW(svc_.start_for(u));

    EXPECT_TRUE(sent_.empty());
    EXPECT_TRUE(tokens_.find_for_user(u.id).has_value());  // code still issued
}

TEST_F(EmailVerificationServiceTest, ResendIsSilentWhenNothingIsDue) {
    // Unknown address.
    EXPECT_NO_THROW(svc_.resend("ghost@e.com"));

    // A Google account never verifies by code.
    User g;
    g.email = "g@e.com";
    g.role = "trainee";
    g.display_name = "Gwen";
    g.auth_provider = "google";
    g.google_sub = "sub-1";
    g.email_verified = true;
    users_.create(g);
    EXPECT_NO_THROW(svc_.resend("g@e.com"));

    // An already-verified local account.
    const User u = make_local_user();
    users_.mark_email_verified(u.id);
    EXPECT_NO_THROW(svc_.resend("u@e.com"));

    EXPECT_TRUE(sent_.empty());
}

TEST_F(EmailVerificationServiceTest, ResendHonoursTheCooldownThenSendsAFreshCode) {
    const User u = make_local_user();
    svc_.start_for(u);
    ASSERT_EQ(sent_.size(), 1u);

    svc_.resend("u@e.com");  // same instant - inside the 60s window
    EXPECT_EQ(sent_.size(), 1u);

    advance_seconds(60);
    svc_.resend("u@e.com");
    ASSERT_EQ(sent_.size(), 2u);

    const auto token = tokens_.find_for_user(u.id);
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(token->issued_at, "2026-09-02 12:01:00");
    EXPECT_EQ(token->code_hash, sha256_hex(last_code()));
}

TEST_F(EmailVerificationServiceTest, ResendResetsTheAttemptCounterAndInvalidatesTheOldCode) {
    const User u = make_local_user();
    svc_.start_for(u);
    const std::string old_code = last_code();
    const std::string wrong = old_code == "000000" ? "111111" : "000000";
    for (int i = 0; i < 3; ++i) {
        kind_thrown_by([&] { svc_.verify("u@e.com", wrong); });
    }
    ASSERT_EQ(tokens_.find_for_user(u.id)->attempts, 3);

    advance_seconds(60);
    svc_.resend("u@e.com");
    const std::string new_code = last_code();

    EXPECT_EQ(tokens_.find_for_user(u.id)->attempts, 0);
    if (new_code != old_code) {
        EXPECT_EQ(kind_thrown_by([&] { svc_.verify("u@e.com", old_code); }),
                  EmailVerificationErrorKind::kCodeMismatch);
    }
    EXPECT_NO_THROW(svc_.verify("u@e.com", new_code));
}

}  // namespace
