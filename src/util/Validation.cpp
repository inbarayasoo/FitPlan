#include "util/Validation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace fitplan::util {

namespace {

// The only hosts we accept a tutorial link from. Kept tiny on purpose: adding a
// site is a deliberate one-line change here, reviewed like any other.
constexpr std::array<std::string_view, 6> kAllowedHosts{
    "youtube.com", "www.youtube.com", "m.youtube.com",
    "youtu.be",    "instagram.com",   "www.instagram.com",
};

// Characters that may appear in a YouTube video id (the "v" parameter). This is
// the URL-safe base64 alphabet; anything else means we did not really find an id.
bool is_video_id_char(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_' || c == '-';
}

// The pieces of a URL we care about. Everything is a view into the original
// string, so `Parsed` must not outlive it.
struct Parsed {
    std::string host;        // lower-cased, no port, no credentials
    std::string_view path;   // starts with '/', or empty
    std::string_view query;  // after '?', without the '?'
};

// A deliberately small https-only URL splitter. Returns nullopt unless the input
// is exactly "https://<authority>[/<path>][?<query>][#fragment]" with a plain
// host authority (no "user:pass@", no IPv6 literal - none of which a video link
// needs).
std::optional<Parsed> parse_https_url(const std::string& url) {
    constexpr std::string_view kScheme = "https://";
    if (url.size() <= kScheme.size() || !url.starts_with(kScheme)) {
        return std::nullopt;
    }

    std::string_view rest{url};
    rest.remove_prefix(kScheme.size());

    // Authority runs up to the first '/', '?' or '#'.
    const std::size_t authority_end = rest.find_first_of("/?#");
    const std::string_view authority = rest.substr(0, authority_end);
    std::string_view after_authority =
        authority_end == std::string_view::npos ? std::string_view{} : rest.substr(authority_end);

    if (authority.empty() || authority.find('@') != std::string_view::npos) {
        return std::nullopt;  // empty host, or "credentials@host" spoofing trick
    }

    // Split off an optional ":port" - we do not police the port, just the host.
    std::string_view host_view = authority.substr(0, authority.find(':'));
    if (host_view.empty()) {
        return std::nullopt;
    }

    Parsed out;
    out.host.reserve(host_view.size());
    for (char c : host_view) {
        out.host.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // Separate path from query, dropping any "#fragment".
    std::string_view path_and_query = after_authority;
    if (const std::size_t hash = path_and_query.find('#'); hash != std::string_view::npos) {
        path_and_query = path_and_query.substr(0, hash);
    }
    if (const std::size_t q = path_and_query.find('?'); q != std::string_view::npos) {
        out.path = path_and_query.substr(0, q);
        out.query = path_and_query.substr(q + 1);
    } else {
        out.path = path_and_query;
    }
    return out;
}

// Value of `key` in an "a=1&b=2" query string, or empty if absent.
std::string_view query_param(std::string_view query, std::string_view key) {
    while (!query.empty()) {
        const std::size_t amp = query.find('&');
        const std::string_view pair = query.substr(0, amp);
        const std::size_t eq = pair.find('=');
        if (eq != std::string_view::npos && pair.substr(0, eq) == key) {
            return pair.substr(eq + 1);
        }
        if (amp == std::string_view::npos) {
            break;
        }
        query.remove_prefix(amp + 1);
    }
    return {};
}

// Trim a candidate id to its leading run of id-characters and accept it only if
// what remains is non-empty and not absurdly long.
std::optional<std::string> clean_video_id(std::string_view candidate) {
    std::size_t len = 0;
    while (len < candidate.size() && is_video_id_char(candidate[len])) {
        ++len;
    }
    const std::string_view id = candidate.substr(0, len);
    if (id.empty() || id.size() > 32) {
        return std::nullopt;
    }
    return std::string{id};
}

bool host_is_youtube(std::string_view host) {
    return host == "youtube.com" || host == "www.youtube.com" || host == "m.youtube.com" ||
           host == "youtu.be";
}

}  // namespace

bool is_allowed_video_url(const std::string& url) {
    const std::optional<Parsed> parsed = parse_https_url(url);
    if (!parsed) {
        return false;
    }
    return std::find(kAllowedHosts.begin(), kAllowedHosts.end(), parsed->host) !=
           kAllowedHosts.end();
}

std::optional<std::string> youtube_embed_url(const std::string& url) {
    const std::optional<Parsed> parsed = parse_https_url(url);
    if (!parsed || !host_is_youtube(parsed->host)) {
        return std::nullopt;
    }

    std::optional<std::string> id;
    if (parsed->host == "youtu.be") {
        // https://youtu.be/<id>
        if (parsed->path.size() > 1) {
            id = clean_video_id(parsed->path.substr(1));
        }
    } else if (parsed->path == "/watch") {
        // https://www.youtube.com/watch?v=<id>
        id = clean_video_id(query_param(parsed->query, "v"));
    } else if (parsed->path.starts_with("/embed/")) {
        id = clean_video_id(parsed->path.substr(std::string_view("/embed/").size()));
    } else if (parsed->path.starts_with("/shorts/")) {
        id = clean_video_id(parsed->path.substr(std::string_view("/shorts/").size()));
    }

    if (!id) {
        return std::nullopt;
    }
    return "https://www.youtube-nocookie.com/embed/" + *id;
}

}  // namespace fitplan::util
