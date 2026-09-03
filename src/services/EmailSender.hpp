#pragma once

#include <functional>
#include <string>

namespace fitplan::services {

// One outbound message. Plain text only - the verification email needs no HTML.
struct EmailMessage {
    std::string to_email;
    std::string to_name;
    std::string subject;
    std::string body;
};

// Sends one email. Throws std::runtime_error if the message could not be handed
// off to the transport. Injected everywhere it is used, so:
//   * main() picks the transport at startup (Brevo API when a key is set, a log
//     line otherwise),
//   * tests pass a lambda that records the message instead of sending it.
using EmailSender = std::function<void(const EmailMessage&)>;

}  // namespace fitplan::services
