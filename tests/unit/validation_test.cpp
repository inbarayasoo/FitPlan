// Unit tests for the video-URL allowlist and the YouTube embed-URL helper.

#include <gtest/gtest.h>

#include <string>

#include "util/Validation.hpp"

namespace {

using fitplan::util::is_allowed_video_url;
using fitplan::util::youtube_embed_url;

TEST(IsAllowedVideoUrl, AcceptsYouTubeAndInstagramHosts) {
    EXPECT_TRUE(is_allowed_video_url("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
    EXPECT_TRUE(is_allowed_video_url("https://youtube.com/watch?v=dQw4w9WgXcQ"));
    EXPECT_TRUE(is_allowed_video_url("https://m.youtube.com/watch?v=dQw4w9WgXcQ"));
    EXPECT_TRUE(is_allowed_video_url("https://youtu.be/dQw4w9WgXcQ"));
    EXPECT_TRUE(is_allowed_video_url("https://www.instagram.com/reel/Cabcdef1234/"));
    EXPECT_TRUE(is_allowed_video_url("https://instagram.com/p/Cabcdef1234/"));
}

TEST(IsAllowedVideoUrl, RejectsWrongSchemeOrNonUrl) {
    EXPECT_FALSE(is_allowed_video_url("http://www.youtube.com/watch?v=dQw4w9WgXcQ"));
    EXPECT_FALSE(is_allowed_video_url("javascript:alert(1)"));
    EXPECT_FALSE(is_allowed_video_url("data:text/html,<script>1</script>"));
    EXPECT_FALSE(is_allowed_video_url("ftp://youtube.com/x"));
    EXPECT_FALSE(is_allowed_video_url("www.youtube.com/watch?v=x"));
    EXPECT_FALSE(is_allowed_video_url("not a url"));
    EXPECT_FALSE(is_allowed_video_url(""));
    EXPECT_FALSE(is_allowed_video_url("https://"));
}

TEST(IsAllowedVideoUrl, RejectsLookalikeAndSpoofedHosts) {
    EXPECT_FALSE(is_allowed_video_url("https://youtube.com.evil.com/watch?v=x"));
    EXPECT_FALSE(is_allowed_video_url("https://notyoutube.com/watch?v=x"));
    EXPECT_FALSE(is_allowed_video_url("https://evil.com/youtube.com"));
    EXPECT_FALSE(is_allowed_video_url("https://youtube.com@evil.com/watch?v=x"));
    EXPECT_FALSE(is_allowed_video_url("https://vimeo.com/123456"));
}

TEST(IsAllowedVideoUrl, IgnoresCaseInHostAndAllowsPort) {
    EXPECT_TRUE(is_allowed_video_url("https://WWW.YouTube.com/watch?v=dQw4w9WgXcQ"));
    EXPECT_TRUE(is_allowed_video_url("https://youtube.com:443/watch?v=dQw4w9WgXcQ"));
}

TEST(YouTubeEmbedUrl, ExtractsIdFromEachYouTubeUrlShape) {
    const std::string expected =
        "https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ";
    EXPECT_EQ(youtube_embed_url("https://www.youtube.com/watch?v=dQw4w9WgXcQ"), expected);
    EXPECT_EQ(youtube_embed_url("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42s"),
              expected);
    EXPECT_EQ(youtube_embed_url("https://youtu.be/dQw4w9WgXcQ?si=abc"), expected);
    EXPECT_EQ(youtube_embed_url("https://m.youtube.com/watch?v=dQw4w9WgXcQ"), expected);
    EXPECT_EQ(youtube_embed_url("https://www.youtube.com/embed/dQw4w9WgXcQ"), expected);
    EXPECT_EQ(youtube_embed_url("https://www.youtube.com/shorts/dQw4w9WgXcQ"), expected);
}

TEST(YouTubeEmbedUrl, ReturnsNulloptForNonYouTubeOrUnparseable) {
    EXPECT_FALSE(youtube_embed_url("https://www.instagram.com/reel/Cabcdef1234/").has_value());
    EXPECT_FALSE(youtube_embed_url("https://www.youtube.com/watch?x=1").has_value());
    EXPECT_FALSE(youtube_embed_url("https://www.youtube.com/").has_value());
    EXPECT_FALSE(youtube_embed_url("http://www.youtube.com/watch?v=dQw4w9WgXcQ").has_value());
}

}  // namespace
