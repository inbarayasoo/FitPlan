#pragma once

#include <string>

#include "services/EmailSender.hpp"

namespace fitplan::services {

struct BrevoConfig {
    std::string api_key;
    std::string from_email;
    std::string from_name;
};

// Builds an EmailSender that posts to Brevo's transactional-email endpoint
// (https://api.brevo.com/v3/smtp/email) over HTTPS via cpr. The returned
// callable throws std::runtime_error on a transport error or a non-2xx reply.
EmailSender make_brevo_email_sender(BrevoConfig config);

}  // namespace fitplan::services
