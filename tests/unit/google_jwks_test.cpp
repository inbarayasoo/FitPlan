#include "services/GoogleJwks.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "RsaTestKey.hpp"

namespace {

using fitplan::services::GoogleJwksCache;
using fitplan::services::JwksDocument;
using fitplan::services::parse_google_jwks;
using fitplan::testutil::make_rsa_test_key;
using fitplan::testutil::RsaTestKey;

// --- parse_google_jwks --------------------------------------------------------

TEST(ParseGoogleJwksTest, ExtractsEveryRsaSigningKeyByKid) {
    const auto k1 = make_rsa_test_key("kid-1");
    const auto k2 = make_rsa_test_key("kid-2");

    const auto keys = parse_google_jwks(RsaTestKey::jwks_document({k1, k2}));

    ASSERT_EQ(keys.size(), 2u);
    EXPECT_NE(keys.at("kid-1").find("BEGIN PUBLIC KEY"), std::string::npos);
    EXPECT_EQ(keys.at("kid-1"), k1.public_pem);
    EXPECT_EQ(keys.at("kid-2"), k2.public_pem);
}

TEST(ParseGoogleJwksTest, RejectsADocumentThatIsNotAnObjectWithAKeysArray) {
    EXPECT_THROW(parse_google_jwks(R"({"not":"keys"})"), std::runtime_error);
    EXPECT_THROW(parse_google_jwks("not json at all"), std::runtime_error);
}

TEST(ParseGoogleJwksTest, SkipsEntriesThatAreNotRsaSignatureKeys) {
    const auto good = make_rsa_test_key("good");
    const std::string doc =
        std::string(R"({"keys":[)") +
        R"({"kty":"EC","kid":"ec","use":"sig","crv":"P-256","x":"x","y":"y"},)" +
        R"({"kty":"RSA","kid":"for-encryption","use":"enc","n":"AAAA","e":"AQAB"},)" +
        good.jwks_entry() + R"(]})";

    const auto keys = parse_google_jwks(doc);

    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys.count("good"), 1u);
}

// --- GoogleJwksCache --------------------------------------------------------

// A stand-in for the network fetch: hands back a document we control, counts how
// often it was called, and can be told to fail or to change what it returns.
class FakeFetcher {
public:
    explicit FakeFetcher(std::string body) : body_(std::move(body)) {}

    JwksDocument operator()() {
        ++calls;
        if (fail) {
            throw std::runtime_error("simulated network failure");
        }
        JwksDocument doc;
        doc.body = body_;
        doc.ttl = ttl;
        return doc;
    }

    void set_body(std::string body) { body_ = std::move(body); }

    int calls = 0;
    bool fail = false;
    std::chrono::seconds ttl{std::chrono::hours{1}};

private:
    std::string body_;
};

// Wraps a shared FakeFetcher in the std::function the cache expects, so the test
// keeps a handle on the call count.
GoogleJwksCache make_cache(const std::shared_ptr<FakeFetcher>& fake,
                           std::chrono::seconds gap = std::chrono::seconds{0}) {
    return GoogleJwksCache([fake] { return (*fake)(); }, gap);
}

TEST(GoogleJwksCacheTest, FetchesOnceThenServesFromMemory) {
    const auto key = make_rsa_test_key("kid-1");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({key}));
    auto cache = make_cache(fake);

    EXPECT_TRUE(cache.public_key_pem("kid-1").has_value());
    EXPECT_TRUE(cache.public_key_pem("kid-1").has_value());

    EXPECT_EQ(fake->calls, 1);
}

TEST(GoogleJwksCacheTest, RefetchesWhenTheKidIsUnknown) {
    const auto key = make_rsa_test_key("kid-1");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({key}));
    auto cache = make_cache(fake);

    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());  // fetch #1
    EXPECT_FALSE(cache.public_key_pem("nope").has_value());  // fetch #2, still missing

    EXPECT_EQ(fake->calls, 2);
}

TEST(GoogleJwksCacheTest, DoesNotRefetchForAnUnknownKidWhileTheCacheIsFresh) {
    const auto key = make_rsa_test_key("kid-1");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({key}));
    auto cache = make_cache(fake, std::chrono::seconds{60});  // wide throttle gap

    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());  // fetch #1
    // The document was just fetched; refetching for a kid it does not contain
    // would only hammer Google, so the throttle suppresses it.
    EXPECT_FALSE(cache.public_key_pem("nope").has_value());
    EXPECT_FALSE(cache.public_key_pem("nope").has_value());

    EXPECT_EQ(fake->calls, 1);
}

TEST(GoogleJwksCacheTest, PicksUpARotatedKeyOnTheRefetch) {
    const auto k1 = make_rsa_test_key("kid-1");
    const auto k2 = make_rsa_test_key("kid-2");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({k1}));
    auto cache = make_cache(fake);

    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());  // fetch #1: only kid-1

    fake->set_body(RsaTestKey::jwks_document({k1, k2}));     // Google rotates in kid-2
    EXPECT_TRUE(cache.public_key_pem("kid-2").has_value());  // fetch #2: now present
}

TEST(GoogleJwksCacheTest, RefetchesAfterTheTtlExpires) {
    const auto key = make_rsa_test_key("kid-1");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({key}));
    fake->ttl = std::chrono::seconds{0};  // every read sees the copy as stale
    auto cache = make_cache(fake);

    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());
    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());

    EXPECT_EQ(fake->calls, 2);
}

TEST(GoogleJwksCacheTest, ServesStaleKeysWhenARefreshFails) {
    const auto key = make_rsa_test_key("kid-1");
    auto fake = std::make_shared<FakeFetcher>(RsaTestKey::jwks_document({key}));
    fake->ttl = std::chrono::seconds{0};
    auto cache = make_cache(fake);

    ASSERT_TRUE(cache.public_key_pem("kid-1").has_value());  // fetch #1 succeeds

    fake->fail = true;
    EXPECT_TRUE(cache.public_key_pem("kid-1").has_value());  // fetch #2 fails -> stale copy
    EXPECT_EQ(fake->calls, 2);
}

TEST(GoogleJwksCacheTest, ThrowsWhenTheVeryFirstFetchFails) {
    auto fake = std::make_shared<FakeFetcher>("{}");
    fake->fail = true;
    auto cache = make_cache(fake);

    EXPECT_THROW(cache.public_key_pem("kid-1"), std::runtime_error);
}

}  // namespace
