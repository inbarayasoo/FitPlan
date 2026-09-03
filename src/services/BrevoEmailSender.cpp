#include "services/BrevoEmailSender.hpp"

#include <cpr/cpr.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace fitplan::services {

namespace {

constexpr const char* kBrevoEmailUrl = "https://api.brevo.com/v3/smtp/email";

}  // namespace

EmailSender make_brevo_email_sender(BrevoConfig config) {
    return [config = std::move(config)](const EmailMessage& msg) {
        nlohmann::json payload;
        payload["sender"] = {{"email", config.from_email}, {"name", config.from_name}};
        payload["to"] =
            nlohmann::json::array({nlohmann::json{{"email", msg.to_email}, {"name", msg.to_name}}});
        payload["subject"] = msg.subject;
        payload["textContent"] = msg.body;

        const cpr::Response res =
            cpr::Post(cpr::Url{kBrevoEmailUrl},
                      cpr::Header{{"api-key", config.api_key},
                                  {"content-type", "application/json"},
                                  {"accept", "application/json"}},
                      cpr::Body{payload.dump()}, cpr::Timeout{std::chrono::seconds{10}});

        if (res.error) {
            throw std::runtime_error("Brevo email send failed: " + res.error.message);
        }
        if (res.status_code / 100 != 2) {
            throw std::runtime_error("Brevo email send returned HTTP " +
                                     std::to_string(res.status_code) + ": " + res.text);
        }
    };
}

}  // namespace fitplan::services
