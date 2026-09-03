#pragma once

#include "services/EmailSender.hpp"

namespace fitplan::services {

// Builds an EmailSender that writes the message to the log instead of sending
// it. Used in development and CI, where FITPLAN_BREVO_API_KEY is unset: email
// verification is still enforced, and the code is read from the server log.
EmailSender make_log_email_sender();

}  // namespace fitplan::services
