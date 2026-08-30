#pragma once

#include <optional>
#include <string>

namespace fitplan::util {

// Input validation shared by the coach controllers. These functions are pure
// (no I/O, no globals) so they are trivial to unit-test.

// True when `url` is a tutorial-video link we are willing to store, embed, or
// link to: it must be an "https://" URL whose host is one of a small, fixed set
// of YouTube / Instagram hostnames. Everything else - other hosts, "http://",
// "javascript:", "data:", a host with embedded credentials ("user@host"), or a
// string that is not a URL at all - returns false. A controller turns a false
// here into a 400 response.
bool is_allowed_video_url(const std::string& url);

// If `url` is a recognizable YouTube watch / share / embed / shorts link,
// returns the "https://www.youtube-nocookie.com/embed/<id>" form that the
// frontend can drop straight into an <iframe>. Returns std::nullopt for a
// non-YouTube link (e.g. Instagram) or anything it cannot parse - the frontend
// then just opens the original URL in a new tab.
//
// Only ever called on a URL that already passed is_allowed_video_url().
std::optional<std::string> youtube_embed_url(const std::string& url);

}  // namespace fitplan::util
