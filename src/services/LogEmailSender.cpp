#include "services/LogEmailSender.hpp"

#include <spdlog/spdlog.h>

namespace fitplan::services {

EmailSender make_log_email_sender() {
    return [](const EmailMessage& msg) {
        spdlog::warn("[email:log] no FITPLAN_BREVO_API_KEY - not sending. to=<{}> subject=\"{}\"",
                     msg.to_email, msg.subject);
        spdlog::warn("[email:log] body:\n{}", msg.body);
    };
}

}  // namespace fitplan::services
