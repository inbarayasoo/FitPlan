#pragma once

// A minimal blocking HTTP/1.1 client over a raw POSIX socket, shared by the
// integration tests. Each test starts a real FitPlanApp on a loopback port in a
// background thread and drives it through these helpers - the same path a real
// client takes, global middleware included (Crow 1.2.x skips global middleware
// in app.handle_full, so an in-process call would not exercise it).

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace fitplan::testutil {

struct HttpResponse {
    int status = 0;
    std::string body;
};

inline HttpResponse http_request(std::uint16_t port, const std::string& method,
                                 const std::string& path, const std::string& body,
                                 const std::string& bearer = "") {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ADD_FAILURE() << "socket() failed";
        return {};
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ADD_FAILURE() << "connect() failed";
        ::close(fd);
        return {};
    }

    std::string req = method + " " + path + " HTTP/1.1\r\n";
    req += "Host: 127.0.0.1\r\n";
    req += "Content-Type: application/json\r\n";
    if (!bearer.empty()) {
        req += "Authorization: Bearer " + bearer + "\r\n";
    }
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += body;
    ::send(fd, req.data(), req.size(), 0);

    std::string raw;
    char buf[4096];
    ssize_t n = 0;
    while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fd);

    HttpResponse res;
    const auto first_space = raw.find(' ');
    if (first_space != std::string::npos) {
        res.status = std::stoi(raw.substr(first_space + 1, 3));
    }
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        res.body = raw.substr(header_end + 4);
    }
    return res;
}

// The first string value for "key":"..." in a flat-ish JSON body.
inline std::string json_string(const std::string& body, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    const auto start = body.find(needle);
    if (start == std::string::npos) {
        return {};
    }
    const auto from = start + needle.size();
    const auto end = body.find('"', from);
    return body.substr(from, end - from);
}

// The first integer value for "key":N in a JSON body. The leading quote in the
// needle keeps "id" from matching inside "exercise_id", and object keys are
// emitted sorted, so on a plan response "id" is the header's, not an item's.
inline long long json_number(const std::string& body, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const auto start = body.find(needle);
    if (start == std::string::npos) {
        return -1;
    }
    return std::strtoll(body.c_str() + start + needle.size(), nullptr, 10);
}

}  // namespace fitplan::testutil
